#include <errno.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/limits.h>
#include <sys/stat.h>
#include <tenok.h>
#include <unistd.h>

#include <arch/port.h>
#include <common/bitops.h>
#include <fs/fs.h>
#include <fs/reg_file.h>
#include <kernel/daemon.h>
#include <kernel/kernel.h>
#include <kernel/pipe.h>
#include <kernel/preempt.h>
#include <mm/mm.h>
#include <mm/slab.h>

static void fs_mount_directory(struct inode *inode_src,
                               struct inode *inode_target);
static int fs_create_file(const char *pathname, mode_t mode, bool create_dirs);
static int fs_open_file(const char *pathname);
ssize_t rootfs_read(struct file *filp, char *buf, size_t size, off_t offset);
ssize_t rootfs_write(struct file *filp,
                     const char *buf,
                     size_t size,
                     off_t offset);

extern struct file *files[FILE_RESERVED_NUM + FILE_MAX];
extern int file_cnt;

struct inode inodes[INODE_MAX];
uint8_t rootfs_blks[FS_BLK_CNT][FS_BLK_SIZE];

uint32_t bitmap_inodes[BITMAP_SIZE(INODE_MAX)];
uint32_t bitmap_blks[BITMAP_SIZE(FS_BLK_CNT)];

struct mount mount_points[MOUNT_MAX + 1]; /* 0 is reserved for the rootfs */
int mount_cnt;

/* Dentry slots released by unlink() are recycled through this list. Without
 * it the space of a removed file could never be reused.
 */
static LIST_HEAD(free_dentries);

/* File descriptor numbers released by unlink() are recycled through this
 * stack, otherwise creating and removing files repeatedly exhausts the file
 * table even though only a few files exist at a time.
 */
static int free_fds[FILE_MAX];
static int free_fd_cnt;

__FILE __stdin = {.fd = STDIN_FILENO};
__FILE __stdout = {.fd = STDOUT_FILENO};
__FILE __stderr = {.fd = STDERR_FILENO};
FILE *stdin = (FILE *) &__stdin;
FILE *stdout = (FILE *) &__stdout;
FILE *stderr = (FILE *) &__stderr;

struct inode *shell_dir_curr;

struct kmem_cache *file_caches;

static struct file_operations rootfs_file_ops = {
    .read = rootfs_read,
    .write = rootfs_write,
};

void link_stdin_dev(char *path)
{
    int fd = fs_open_file(path);
    if (fd < 0)
        halt();
    files[STDIN_FILENO] = files[fd];
}

void link_stdout_dev(char *path)
{
    int fd = fs_open_file(path);
    if (fd < 0)
        halt();
    files[STDOUT_FILENO] = files[fd];
}

void link_stderr_dev(char *path)
{
    int fd = fs_open_file(path);
    if (fd < 0)
        halt();
    files[STDERR_FILENO] = files[fd];
}

int register_chrdev(char *name, struct file_operations *fops)
{
    char dev_path[100] = {0};
    snprintf(dev_path, PATH_MAX, "/dev/%s", name);

    /* Create new character device file */
    int fd = fs_create_file(dev_path, S_IFCHR | FS_DEFAULT_FILE_MODE, true);

    /* Link the file operations */
    files[fd]->f_op = fops;

    return 0;
}

int register_blkdev(char *name, struct file_operations *fops)
{
    char dev_path[100] = {0};
    snprintf(dev_path, PATH_MAX, "/dev/%s", name);

    /* Create new block device file */
    int fd = fs_create_file(dev_path, S_IFBLK | FS_DEFAULT_FILE_MODE, true);

    /* Link the file operations */
    files[fd]->f_op = fops;

    return 0;
}

/* Dispatch a file descriptor number of the file table. The function returns
 * a negative number if the file table is full.
 */
static int fs_alloc_fd(void)
{
    /* Reuse a file descriptor number that was released before */
    if (free_fd_cnt > 0)
        return free_fds[--free_fd_cnt];

    /* Dispatch a brand new file descriptor number */
    if (file_cnt >= FILE_MAX)
        return -ENFILE;

    int fd = file_cnt + FILE_RESERVED_NUM;
    file_cnt++;

    return fd;
}

/* Give a file descriptor number back to the file table */
static void fs_release_fd(int fd)
{
    files[fd] = NULL;

    if (free_fd_cnt < FILE_MAX)
        free_fds[free_fd_cnt++] = fd;
}

struct file *fs_alloc_file(void)
{
    preempt_disable();
    struct file *new_file = kmem_cache_alloc(file_caches, 0);
    memset(new_file, 0, sizeof(*new_file));
    preempt_enable();

    return new_file;
}

static struct inode *fs_alloc_inode(void)
{
    struct inode *new_inode = NULL;

    /* Find the first free inode */
    int free_idx = find_first_zero_bit(bitmap_inodes, INODE_MAX);
    if (free_idx < INODE_MAX) {
        /* Allocate new inode */
        new_inode = &inodes[free_idx];
        new_inode->i_ino = free_idx;
        mount_points[RDEV_ROOTFS].super_blk.s_inode_cnt++;

        /* Update the inode bitmap */
        bitmap_set_bit(bitmap_inodes, free_idx);
    }

    return new_inode;
}

static void fs_free_inode(struct inode *inode)
{
    bitmap_clear_bit(bitmap_inodes, inode->i_ino);
    mount_points[RDEV_ROOTFS].super_blk.s_inode_cnt--;
}

static uint8_t *fs_alloc_block(void)
{
    uint8_t *new_block = NULL;

    /* Find the first free block */
    int free_idx = find_first_zero_bit(bitmap_blks, FS_BLK_CNT);
    if (free_idx < FS_BLK_CNT) {
        /* Allocate new block */
        new_block = (uint8_t *) &rootfs_blks[free_idx];
        mount_points[RDEV_ROOTFS].super_blk.s_blk_cnt++;

        /* Update the block bitmap */
        bitmap_set_bit(bitmap_blks, free_idx);
    }

    return new_block;
}

static void fs_free_block(uint32_t blk_addr)
{
    uint32_t blk_base = (uint32_t) rootfs_blks;
    uint32_t blk_end = blk_base + (FS_BLK_CNT * FS_BLK_SIZE);

    /* The address must point to a block of the rootfs */
    if ((blk_addr < blk_base) || (blk_addr >= blk_end))
        return;

    int blk_idx = (blk_addr - blk_base) / FS_BLK_SIZE;

    if (bitmap_get_bit(bitmap_blks, blk_idx)) {
        bitmap_clear_bit(bitmap_blks, blk_idx);
        mount_points[RDEV_ROOTFS].super_blk.s_blk_cnt--;
    }
}

void rootfs_init(void)
{
    file_caches = kmem_cache_create("file_cache", sizeof(struct file),
                                    sizeof(uint32_t), 0, NULL);

    /* Configure the super block */
    struct super_block *rootfs_super_blk = &mount_points[RDEV_ROOTFS].super_blk;
    rootfs_super_blk->s_inode_cnt = 0;
    rootfs_super_blk->s_blk_cnt = 0;
    rootfs_super_blk->s_rd_only = false;
    rootfs_super_blk->s_sb_addr = (uint32_t) rootfs_super_blk;
    rootfs_super_blk->s_ino_addr = (uint32_t) inodes;
    rootfs_super_blk->s_blk_addr = (uint32_t) rootfs_blks;

    /* Configure the root directory inode */
    struct inode *inode_root = fs_alloc_inode();
    inode_root->i_mode = S_IFDIR | FS_DEFAULT_DIR_MODE;
    inode_root->i_rdev = RDEV_ROOTFS;
    inode_root->i_sync = true;
    inode_root->i_size = 0;
    inode_root->i_blocks = 0;
    inode_root->i_data = (uint32_t) NULL;
    INIT_LIST_HEAD(&inode_root->i_dentry);

    shell_dir_curr = inode_root;

    /* Create rootfs device file */
    struct file *rootfs_file = fs_alloc_file();
    rootfs_file->f_op = &rootfs_file_ops;

    int fd = fs_alloc_fd();
    files[fd] = rootfs_file;

    /* Mount the rootfs */
    mount_points[RDEV_ROOTFS].dev_file = rootfs_file;
    mount_cnt = 1;
}

static bool rootfs_mem_check(uint32_t addr)
{
    bool pass = false;

    uint32_t inode_start_addr = (uint32_t) inodes;
    uint32_t inode_end_addr =
        (uint32_t) inodes + (sizeof(struct inode) * INODE_MAX);

    if ((addr >= inode_start_addr) && (addr <= inode_end_addr))
        pass = true;

    uint32_t blk_start_addr = (uint32_t) rootfs_blks;
    uint32_t blk_end_addr = (uint32_t) rootfs_blks + (FS_BLK_CNT * FS_BLK_SIZE);

    if ((addr >= blk_start_addr) && (addr <= blk_end_addr))
        pass = true;

    return pass;
}

ssize_t rootfs_read(struct file *filp, char *buf, size_t size, off_t offset)
{
    /* Offset is used as the read address */
    uint8_t *read_addr = (uint8_t *) offset;

    if (rootfs_mem_check((uint32_t) read_addr) == false)
        return -EFAULT;

    memcpy(buf, read_addr, size);

    return size;
}

ssize_t rootfs_write(struct file *filp,
                     const char *buf,
                     size_t size,
                     off_t offset)
{
    /* Offset is used as the write address */
    uint8_t *write_addr = (uint8_t *) offset;

    if (rootfs_mem_check((uint32_t) write_addr) == false)
        return -EFAULT;

    memcpy(write_addr, buf, size);

    return size;
}

static void fs_read_list(struct file *dev_file,
                         uint32_t list_addr,
                         struct list_head *list)
{
    /* Read list of the file */
    dev_file->f_op->read(dev_file, (char *) list, sizeof(struct list_head),
                         list_addr);
}

static void fs_read_dentry(struct file *dev_file,
                           uint32_t dentry_addr,
                           struct dentry *dentry)
{
    /* Read dentry of the file */
    dev_file->f_op->read(dev_file, (char *) dentry, sizeof(struct dentry),
                         dentry_addr);
}

static void fs_read_inode(uint8_t rdev,
                          struct file *dev_file,
                          uint32_t inode_num,
                          struct inode *inode)
{
    /* Calculate the inode address */
    struct super_block *super_blk = &mount_points[rdev].super_blk;
    uint32_t inode_addr =
        super_blk->s_ino_addr + (sizeof(struct inode) * inode_num);

    /* Read the inode */
    dev_file->f_op->read(dev_file, (char *) inode, sizeof(struct inode),
                         inode_addr);
}

/* Search a file under the given directory
 * Input : Directory inode, file name
 * Output: File inode
 */
static struct inode *fs_search_file(struct inode *inode_dir,
                                    const char *file_name)
{
    /* Return current inode. Note that "." and ".." belong to the directory
     * itself, so they must be handled before the emptiness check, otherwise
     * they are unreachable from an empty directory.
     */
    if (strncmp(".", file_name, NAME_MAX) == 0)
        return inode_dir;

    /* Return parent inode */
    if (strncmp("..", file_name, NAME_MAX) == 0)
        return &inodes[inode_dir->i_parent];

    /* Mount directory to synchronize it */
    if (inode_dir->i_sync == false)
        fs_mount_directory(inode_dir, inode_dir);

    /* The dentry table is currently empty */
    if (list_empty(&inode_dir->i_dentry))
        return NULL;

    /* Traverse the dentry list */
    struct dentry *dentry;
    list_for_each_entry (dentry, &inode_dir->i_dentry, d_list) {
        /* Compare the file name with the dentry */
        if (strcmp(dentry->d_name, file_name) == 0)
            return &inodes[dentry->d_inode];
    }

    return NULL;
}

static int fs_calculate_dentry_blocks(size_t block_size, size_t dentry_cnt)
{
    /* Calculate how many dentries a block can hold */
    int dentry_per_blk = block_size / sizeof(struct dentry);

    /* Calculate how many blocks is required for storing N dentries */
    int blocks = dentry_cnt / dentry_per_blk;
    if (dentry_cnt % dentry_per_blk)
        blocks++;

    return blocks;
}

/* Dentries are carved out of the file system blocks. Every block is split
 * into a fixed number of dentry slots and the unused ones are kept on a
 * global free list, so that a slot released by unlink() can be reused no
 * matter which directory it originally belonged to.
 */
static struct dentry *fs_allocate_dentry(void)
{
    /* Reuse a dentry slot that was released before */
    if (!list_empty(&free_dentries)) {
        struct list_head *slot = free_dentries.next;
        list_del(slot);
        return list_entry(slot, struct dentry, d_list);
    }

    /* No free slot is left, carve a new block into dentry slots */
    struct dentry *dentries = (struct dentry *) fs_alloc_block();
    if (!dentries)
        return NULL;

    /* Keep the first slot and release the remaining ones */
    int dentry_per_blk = FS_BLK_SIZE / sizeof(struct dentry);
    for (int i = 1; i < dentry_per_blk; i++)
        list_add_tail(&dentries[i].d_list, &free_dentries);

    return &dentries[0];
}

static void fs_free_dentry(struct dentry *dentry)
{
    list_add(&dentry->d_list, &free_dentries);
}

/* Refresh the size, the block count and the data pointer of a directory
 * after its dentry list is modified
 */
static void fs_dir_update(struct inode *inode_dir)
{
    int dentry_cnt = 0;
    struct dentry *dentry;

    list_for_each_entry (dentry, &inode_dir->i_dentry, d_list)
        dentry_cnt++;

    inode_dir->i_size = dentry_cnt * sizeof(struct dentry);
    inode_dir->i_blocks = fs_calculate_dentry_blocks(FS_BLK_SIZE, dentry_cnt);

    /* The data pointer of a directory addresses its first dentry */
    if (list_empty(&inode_dir->i_dentry)) {
        inode_dir->i_data = (uint32_t) NULL;
    } else {
        struct dentry *first =
            list_first_entry(&inode_dir->i_dentry, struct dentry, d_list);
        inode_dir->i_data = (uint32_t) first;
    }
}

uint32_t fs_file_append_block(struct inode *inode)
{
    uint8_t rdev = inode->i_rdev;

    /* The function currently only supports rootfs */
    if (rdev != RDEV_ROOTFS) {
        return (uint32_t) NULL;
    }

    /* Exceeded the maximum block count */
    if (mount_points[rdev].super_blk.s_blk_cnt >= FS_BLK_CNT) {
        return (uint32_t) NULL;
    }

    /* Allocate new block */
    uint32_t new_blk = (uint32_t) fs_alloc_block();
    if (new_blk == (uint32_t) NULL)
        return (uint32_t) NULL;

    ((struct block_header *) new_blk)->b_next = (uint32_t) NULL;

    /* the file has never been allocated with blocks */
    if (inode->i_blocks == 0) {
        /* initialize the inode data pointer */
        inode->i_data = new_blk;
        inode->i_blocks++;
        return new_blk;
    }

    struct block_header *blk_head;

    /* Iterate to the last block of the file */
    blk_head = (struct block_header *) inode->i_data;
    for (int i = 1; i < inode->i_blocks; i++) {
        blk_head = (struct block_header *) blk_head->b_next;
    }

    /* Append new block */
    blk_head->b_next = new_blk;
    inode->i_blocks++;

    return new_blk;
}

/* Release every block owned by a regular file */
static void fs_free_file_blocks(struct inode *inode)
{
    /* Files provided by an external storage own no rootfs block */
    if (inode->i_rdev != RDEV_ROOTFS || !S_ISREG(inode->i_mode))
        return;

    uint32_t blk_addr = inode->i_data;

    for (uint32_t i = 0; (i < inode->i_blocks) && blk_addr; i++) {
        uint32_t next_blk = ((struct block_header *) blk_addr)->b_next;
        fs_free_block(blk_addr);
        blk_addr = next_blk;
    }

    inode->i_data = (uint32_t) NULL;
    inode->i_blocks = 0;
    inode->i_size = 0;
}

/* Create a file under the given directory (currently only supports rootfs) */
static struct inode *fs_add_file(struct inode *inode_dir,
                                 char *file_name,
                                 mode_t mode)
{
    int fd = -1;
    struct inode *new_inode = NULL;
    struct dentry *new_dentry = NULL;

    /* inodes table is full */
    if (mount_points[0].super_blk.s_inode_cnt >= INODE_MAX)
        goto failed;

    /* Dispatch new file descriptor number */
    fd = fs_alloc_fd();
    if (fd < 0)
        goto failed;

    /* Allocate new inode for the file */
    new_inode = fs_alloc_inode();
    if (!new_inode)
        goto failed;

    new_inode->i_mode = mode;
    new_inode->i_rdev = RDEV_ROOTFS;
    new_inode->i_parent = inode_dir->i_ino;
    new_inode->i_fd = fd;
    new_inode->i_sync = true;
    new_inode->i_size = 0;
    new_inode->i_blocks = 0;
    new_inode->i_data = (uint32_t) NULL;

    /* Configure new dentry */
    new_dentry = fs_allocate_dentry();
    if (!new_dentry)
        goto failed;

    new_dentry->d_inode = new_inode->i_ino;               /* file inode */
    new_dentry->d_parent = inode_dir->i_ino;              /* parent inode */
    strncpy(new_dentry->d_name, file_name, NAME_MAX - 1); /* file name */
    new_dentry->d_name[NAME_MAX - 1] = '\0';

    /* File instantiation */
    int result = 0;

    switch (mode & S_IFMT) {
    case S_IFIFO: {
        /* Named pipe */
        struct pipe *pipe = kmalloc(sizeof(struct pipe));
        struct kfifo *pipe_fifo = kfifo_alloc(1, PIPE_BUF);

        /* Allocation failure */
        if (!pipe || !pipe_fifo) {
            if (pipe)
                kfree(pipe);
            if (pipe_fifo)
                kfifo_free(pipe_fifo);
            goto failed;
        }

        pipe->fifo = pipe_fifo;
        result = fifo_init(fd, (struct file **) &files, new_inode, pipe);

        break;
    }
    case S_IFCHR:
    case S_IFBLK:
    case S_IFDIR: {
        /* Character device, block device and directory. Note that a
         * directory owns a file object as well, otherwise every place that
         * touches files[fd] would have to special case it.
         */
        struct file *dev_file = fs_alloc_file();

        /* Allocation failure */
        if (!dev_file)
            goto failed;

        dev_file->f_inode = new_inode;
        files[fd] = dev_file;

        if (S_ISDIR(mode))
            INIT_LIST_HEAD(&new_inode->i_dentry);

        break;
    }
    case S_IFREG: {
        /* Regular file */
        struct reg_file *reg_file = kmalloc(sizeof(struct reg_file));

        /* Allocation failure */
        if (!reg_file)
            goto failed;

        result = reg_file_init((struct file **) &files, new_inode, reg_file);

        break;
    }
    default:
        result = -1;
    }

    if (result != 0)
        goto failed;

    /* Initialize file events */
    files[fd]->f_events = 0;

    /* insert the new file under the directory */
    list_add_tail(&new_dentry->d_list, &inode_dir->i_dentry);

    /* Update the inode size and block information */
    fs_dir_update(inode_dir);

    return new_inode;

failed:
    if (new_dentry)
        fs_free_dentry(new_dentry);
    if (new_inode)
        fs_free_inode(new_inode);
    if (fd >= 0)
        fs_release_fd(fd);

    return NULL;
}

/* Must be called by the fs_mount_directory() function since current design only
 * supports mounting a whole directory */
static struct inode *fs_mount_file(struct inode *inode_dir,
                                   struct inode *mnt_inode,
                                   struct dentry *mnt_dentry)
{
    /* inodes table is full */
    if (mount_points[0].super_blk.s_inode_cnt >= INODE_MAX)
        return NULL;

    /* Dispatch new file descriptor number */
    int fd = fs_alloc_fd();
    if (fd < 0)
        return NULL;

    /* Allocate new inode for the file */
    struct inode *new_inode = fs_alloc_inode();
    new_inode->i_mode = mnt_inode->i_mode;
    new_inode->i_rdev = mnt_inode->i_rdev;
    new_inode->i_parent = inode_dir->i_ino;
    new_inode->i_fd = fd;
    new_inode->i_size = mnt_inode->i_size;
    new_inode->i_blocks = mnt_inode->i_blocks;
    new_inode->i_data = mnt_inode->i_data;
    new_inode->i_sync = false; /* Synchronized when the file is open */
    INIT_LIST_HEAD(&new_inode->i_dentry);

    /* Configure the new dentry */
    struct dentry *new_dentry = fs_allocate_dentry();
    if (!new_dentry)
        return NULL;

    new_dentry->d_inode = new_inode->i_ino;  /* File inode */
    new_dentry->d_parent = inode_dir->i_ino; /* Parent inode */
    strncpy(new_dentry->d_name, mnt_dentry->d_name, NAME_MAX); /* File name */

    /* Insert the new file under current directory */
    list_add_tail(&new_dentry->d_list, &inode_dir->i_dentry);

    /* Update the inode size and block information */
    fs_dir_update(inode_dir);

    return new_inode;
}

static bool fs_sync_file(struct inode *inode)
{
    /* rootfs files does not require synchronization */
    if (inode->i_rdev == RDEV_ROOTFS)
        return false;

    /* Only regular files require synchronization */
    if (!S_ISREG(inode->i_mode))
        return false;

    /* The file is already synchronized */
    if (inode->i_sync == true)
        return false;

    /* Dispatch new file descriptor number */
    int fd = fs_alloc_fd();
    if (fd < 0)
        return false;

    inode->i_fd = fd;

    /* Create new regular file */
    struct reg_file *reg_file = kmalloc(sizeof(struct reg_file));
    int result = reg_file_init((struct file **) &files, inode, reg_file);

    /* Failed to create new regular file */
    if (result != 0)
        return false;

    /* Update file synchronization flag */
    inode->i_sync = true;

    return true;
}

/* Mount a directory from an external storage under the rootfs
 * Input: "inode_src" (external directory to mount) and "inode_target"
 * (where to mount on the rootfs)
 */
static void fs_mount_directory(struct inode *inode_src,
                               struct inode *inode_target)
{
    /* Nothing to mount */
    if (inode_src->i_size == 0)
        return;

    const uint32_t sb_size = sizeof(struct super_block);
    const uint32_t inode_size = sizeof(struct inode);
    const uint32_t dentry_size = sizeof(struct dentry);

    off_t inode_addr;
    struct inode inode;

    off_t dentry_addr;
    struct dentry dentry;

    /* Load the driver file of the storage device */
    struct file *dev_file = mount_points[inode_src->i_rdev].dev_file;
    ssize_t (*dev_read)(struct file * filp, char *buf, size_t size,
                        off_t offset) = dev_file->f_op->read;

    dentry_addr = (uint32_t) inode_src->i_data;

    /* Initialize the mount target inode */
    if (inode_target->i_sync == false) {
        inode_target->i_size = 0;
        inode_target->i_blocks = 0;
    }

    while (1) {
        /* Load the dentry from the storage device */
        dev_read(dev_file, (char *) &dentry, dentry_size, dentry_addr);

        /* Load the file inode from the storage device */
        inode_addr = sb_size + (inode_size * dentry.d_inode);
        dev_read(dev_file, (char *) &inode, inode_size, inode_addr);

        /* Overwrite the device number */
        inode.i_rdev = inode_src->i_rdev;

        /* Mount the file */
        fs_mount_file(inode_target, &inode, &dentry);

        /* Calculate the address of the next dentry to read */
        dentry_addr =
            (off_t) list_entry(dentry.d_list.next, struct dentry, d_list);

        /* Stop looping when returned back to the list head */
        if ((uint32_t) dentry.d_list.next < (sb_size + inode_size * INODE_MAX))
            break; /* no more dentry to read */
    }

    /* The directory is now synchronized */
    inode_target->i_sync = true;
}

/* Resolve a pathname into the inode of its parent directory together with
 * the last entry of the path. A relative pathname is resolved from the
 * current working directory.
 *
 * Input : Pathname and whether the missing directories should be created
 * Output: Parent directory inode and the last entry name of the path
 */
static int fs_path_lookup(const char *pathname,
                          struct inode **parent,
                          char *name,
                          bool create_dirs)
{
    char path[PATH_MAX];

    if (!pathname || pathname[0] == '\0')
        return -ENOENT;

    if (strlen(pathname) >= PATH_MAX)
        return -ENAMETOOLONG;

    strcpy(path, pathname);

    /* An absolute path is resolved from the root directory while a relative
     * one is resolved from the current working directory
     */
    struct inode *inode_dir = (path[0] == '/') ? &inodes[0] : shell_dir_curr;

    /* Get rid of the leading slashes */
    char *p = path;
    while (*p == '/')
        p++;

    name[0] = '\0';

    while (*p != '\0') {
        /* Split the next entry of the path */
        char *entry = p;
        while ((*p != '\0') && (*p != '/'))
            p++;

        size_t entry_len = p - entry;

        /* Get rid of the successive slashes */
        while (*p == '/')
            p++;

        if (entry_len >= NAME_MAX)
            return -ENAMETOOLONG;

        /* The end of the path is reached, the entry is the file name */
        if (*p == '\0') {
            memcpy(name, entry, entry_len);
            name[entry_len] = '\0';
            *parent = inode_dir;
            return 0;
        }

        /* The entry is an intermediate directory of the path */
        char entry_name[NAME_MAX];
        memcpy(entry_name, entry, entry_len);
        entry_name[entry_len] = '\0';

        struct inode *inode = fs_search_file(inode_dir, entry_name);

        if (!inode) {
            /* The directory does not exist */
            if (!create_dirs)
                return -ENOENT;

            /* Create the missing directory */
            inode = fs_add_file(inode_dir, entry_name,
                                S_IFDIR | FS_DEFAULT_DIR_MODE);
            if (!inode)
                return -ENOSPC;
        }

        /* Failed, not a directory */
        if (!S_ISDIR(inode->i_mode))
            return -ENOTDIR;

        inode_dir = inode;
    }

    /* The pathname refers to the root or the current working directory */
    *parent = inode_dir;

    return 0;
}

/* Resolve a pathname into an inode, no matter it is a file or a directory
 * Input : Pathname
 * Output: File inode, NULL is returned if the file does not exist
 */
static struct inode *fs_resolve_path(const char *pathname)
{
    struct inode *inode_dir;
    char name[NAME_MAX];

    if (fs_path_lookup(pathname, &inode_dir, name, false) != 0)
        return NULL;

    /* The path refers to a directory itself, e.g., "/" */
    if (name[0] == '\0')
        return inode_dir;

    return fs_search_file(inode_dir, name);
}

/* Create a file by given the pathname
 * Input : Path name and file type
 * Output: File descriptor number
 */
static int fs_create_file(const char *pathname, mode_t mode, bool create_dirs)
{
    struct inode *inode_dir;
    char name[NAME_MAX];

    int retval = fs_path_lookup(pathname, &inode_dir, name, create_dirs);
    if (retval < 0)
        return retval;

    /* The pathname does not contain any file name */
    if (name[0] == '\0')
        return -EEXIST;

    /* File with the same name already exists */
    if (fs_search_file(inode_dir, name))
        return -EEXIST;

    /* Create new inode for the file */
    struct inode *inode = fs_add_file(inode_dir, name, mode);
    if (!inode)
        return -ENOSPC;

    return inode->i_fd;
}

/* Open a file by given a pathname
 * Input : Pathname
 * Output: File descriptor number
 */
static int fs_open_file(const char *pathname)
{
    struct inode *inode = fs_resolve_path(pathname);

    /* File not found */
    if (!inode)
        return -ENOENT;

    /* A directory can only be opened with opendir() */
    if (S_ISDIR(inode->i_mode))
        return -EISDIR;

    /* Check if the file requires synchronization */
    if ((inode->i_rdev != RDEV_ROOTFS) && (inode->i_sync == false)) {
        /* Synchronize the file */
        if (!fs_sync_file(inode))
            return -ENFILE;
    }

    /* File is open successfully */
    return inode->i_fd;
}

/* Search the dentry of a file under the given directory
 * Input : Directory inode and the file name
 * Output: The dentry of the file, NULL is returned if it does not exist
 */
static struct dentry *fs_search_dentry(struct inode *inode_dir,
                                       const char *name)
{
    /* Mount the directory to synchronize it */
    if (inode_dir->i_sync == false)
        fs_mount_directory(inode_dir, inode_dir);

    struct dentry *dentry;
    list_for_each_entry (dentry, &inode_dir->i_dentry, d_list) {
        if (strcmp(dentry->d_name, name) == 0)
            return dentry;
    }

    return NULL;
}

/* Remove a regular file or an empty directory from the file system
 * Input : Pathname and whether the pathname must refer to a directory
 */
static int fs_remove(const char *pathname, bool rm_dir)
{
    struct inode *inode_dir;
    char name[NAME_MAX];

    int retval = fs_path_lookup(pathname, &inode_dir, name, false);
    if (retval < 0)
        return retval;

    /* The root, "." and ".." can never be removed */
    if ((name[0] == '\0') || (strcmp(name, ".") == 0) ||
        (strcmp(name, "..") == 0))
        return -EBUSY;

    /* Search the dentry of the file to remove */
    struct dentry *dentry = fs_search_dentry(inode_dir, name);
    if (!dentry)
        return -ENOENT;

    struct inode *inode = &inodes[dentry->d_inode];

    /* Files provided by a read-only storage cannot be removed */
    if (inode->i_rdev != RDEV_ROOTFS)
        return -EROFS;

    /* The current working directory must stay reachable */
    if (inode == shell_dir_curr)
        return -EBUSY;

    if (rm_dir) {
        if (!S_ISDIR(inode->i_mode))
            return -ENOTDIR;

        /* Only an empty directory can be removed */
        if (!list_empty(&inode->i_dentry))
            return -ENOTEMPTY;
    } else {
        if (S_ISDIR(inode->i_mode))
            return -EISDIR;

        /* Device files and named pipes are owned by the kernel */
        if (!S_ISREG(inode->i_mode))
            return -EPERM;
    }

    /* Removing a file that is still open is not supported, as there is no
     * reference count to defer the release with
     */
    if (files[inode->i_fd] && file_is_opened(files[inode->i_fd]))
        return -EBUSY;

    /* Release the resources owned by the file */
    if (S_ISREG(inode->i_mode)) {
        fs_free_file_blocks(inode);

        if (files[inode->i_fd])
            kfree(container_of(files[inode->i_fd], struct reg_file, file));
    } else if (files[inode->i_fd]) {
        kmem_cache_free(file_caches, files[inode->i_fd]);
    }

    fs_release_fd(inode->i_fd);

    /* Detach the file from its parent directory */
    list_del(&dentry->d_list);
    fs_free_dentry(dentry);
    fs_dir_update(inode_dir);

    fs_free_inode(inode);

    return 0;
}

/* Return the information of the file specified by the pathname */
static int fs_stat(const char *pathname, struct stat *statbuf)
{
    struct inode *inode = fs_resolve_path(pathname);

    if (!inode)
        return -ENOENT;

    fs_fill_stat(statbuf, inode);

    return 0;
}

/* Rename a file or move it into another directory */
static int fs_rename(const char *oldpath, const char *newpath)
{
    struct inode *old_dir, *new_dir;
    char old_name[NAME_MAX], new_name[NAME_MAX];

    int retval = fs_path_lookup(oldpath, &old_dir, old_name, false);
    if (retval < 0)
        return retval;

    retval = fs_path_lookup(newpath, &new_dir, new_name, false);
    if (retval < 0)
        return retval;

    /* Neither the source nor the destination can be the root */
    if ((old_name[0] == '\0') || (new_name[0] == '\0'))
        return -EBUSY;

    /* Search the dentry of the file to rename */
    struct dentry *dentry = fs_search_dentry(old_dir, old_name);
    if (!dentry)
        return -ENOENT;

    struct inode *inode = &inodes[dentry->d_inode];

    /* Files provided by a read-only storage cannot be renamed */
    if (inode->i_rdev != RDEV_ROOTFS)
        return -EROFS;

    /* Moving a directory into its own subtree would create a loop on the
     * dentry tree, which hangs every path walk afterwards
     */
    if (S_ISDIR(inode->i_mode)) {
        struct inode *ancestor = new_dir;

        while (1) {
            if (ancestor == inode)
                return -EINVAL;

            if (ancestor->i_ino == 0)
                break;

            ancestor = &inodes[ancestor->i_parent];
        }
    }

    /* Handle the file the new pathname is pointing to */
    struct inode *inode_new = fs_search_file(new_dir, new_name);
    if (inode_new) {
        /* The source and the destination are the same file */
        if (inode_new == inode)
            return 0;

        /* Overwriting a directory is not supported */
        if (S_ISDIR(inode_new->i_mode))
            return -EEXIST;

        /* Remove the file that is being overwritten */
        retval = fs_remove(newpath, false);
        if (retval < 0)
            return retval;
    }

    /* Move the dentry into the new directory */
    list_del(&dentry->d_list);
    strncpy(dentry->d_name, new_name, NAME_MAX - 1);
    dentry->d_name[NAME_MAX - 1] = '\0';
    dentry->d_parent = new_dir->i_ino;
    list_add_tail(&dentry->d_list, &new_dir->i_dentry);

    inode->i_parent = new_dir->i_ino;

    fs_dir_update(old_dir);
    fs_dir_update(new_dir);

    return 0;
}

/* Create a directory by given a pathname. Unlike fs_create_file(), the
 * missing directories of the path are never created implicitly.
 */
static int fs_mkdir(const char *pathname)
{
    struct inode *inode_dir;
    char name[NAME_MAX];

    int retval = fs_path_lookup(pathname, &inode_dir, name, false);
    if (retval < 0)
        return retval;

    /* The pathname refers to an existed directory */
    if (name[0] == '\0')
        return -EEXIST;

    /* File with the same name already exists */
    if (fs_search_file(inode_dir, name))
        return -EEXIST;

    if (!fs_add_file(inode_dir, name, S_IFDIR | FS_DEFAULT_DIR_MODE))
        return -ENOSPC;

    return 0;
}

/* Input : File pathname
 * Output: Directory inode
 */
struct inode *fs_open_directory(const char *pathname)
{
    struct inode *inode = fs_resolve_path(pathname);

    /* Directory does not exist or the path refers to a file */
    if (!inode || !S_ISDIR(inode->i_mode))
        return NULL;

    /* Synchronize the directory */
    if (inode->i_sync == false)
        fs_mount_directory(inode, inode);

    return inode;
}

static int fs_mount(const char *source, const char *target)
{
    /* Get the file of the storage to mount */
    int source_fd = fs_open_file(source);
    if (source_fd < 0)
        return -1;

    /* Get the directory inode for mounting the storage */
    struct inode *target_inode = fs_open_directory(target);
    if (target_inode == NULL)
        return -1;

    /* Get storage device file */
    struct file *dev_file = files[source_fd];
    mount_points[mount_cnt].dev_file = dev_file;

    /* Get the function pointer of the device read() */
    ssize_t (*dev_read)(struct file * filp, char *buf, size_t size,
                        off_t offset) = dev_file->f_op->read;

    /* Calculate the start address of the super block, inode table, and block
     * region */
    const uint32_t sb_size = sizeof(struct super_block);
    const uint32_t inode_size = sizeof(struct inode);
    off_t super_blk_addr = 0;
    off_t inodes_addr = super_blk_addr + sb_size;

    /* Read the super block from the device */
    dev_read(dev_file, (char *) &mount_points[mount_cnt].super_blk, sb_size,
             super_blk_addr);

    /* Read the root inode of the storage */
    struct inode inode_root;
    dev_read(dev_file, (char *) &inode_root, inode_size, inodes_addr);

    /* Overwrite the device number */
    inode_root.i_rdev = mount_cnt;

    /* Mount the root directory */
    fs_mount_directory(&inode_root, target_inode);

    mount_cnt++;

    return 0;
}

char *fs_getcwd(char *buf, size_t len)
{
    char old_path[PATH_MAX] = {0};
    char new_path[PATH_MAX] = {0};

    struct inode *inode = shell_dir_curr;

    while (inode->i_ino != 0) {
        /* Switch to the parent directory */
        uint32_t inode_prev = inode->i_ino;
        inode = &inodes[inode->i_parent];

        /* Search the name of the directory that was left behind */
        struct dentry *dentry;
        list_for_each_entry (dentry, &inode->i_dentry, d_list) {
            if (dentry->d_inode == inode_prev) {
                strncpy(old_path, new_path, PATH_MAX - 1);
                old_path[PATH_MAX - 1] = '\0';
                snprintf(new_path, PATH_MAX, "/%s%s", dentry->d_name, old_path);
                break;
            }
        }
    }

    /* The root directory has no name of its own */
    snprintf(buf, len, "%s", (new_path[0] == '\0') ? "/" : new_path);

    return buf;
}

int fs_chdir(const char *path)
{
    struct inode *inode = fs_resolve_path(path);

    /* Directory not found */
    if (!inode)
        return -ENOENT;

    /* Not a directory */
    if (!S_ISDIR(inode->i_mode))
        return -ENOTDIR;

    /* Synchronize the directory */
    if (inode->i_sync == false)
        fs_mount_directory(inode, inode);

    shell_dir_curr = inode;

    return 0;
}

int fs_read_dir(DIR *dirp, struct dirent *dirent)
{
    /* No dentry to read */
    if (dirp->dentry_list == &dirp->inode_dir->i_dentry)
        return -1;

    /* Read the dentry */
    struct dentry *dentry =
        list_entry(dirp->dentry_list, struct dentry, d_list);

    /* Copy dirent data */
    dirent->d_ino = dentry->d_inode;
    dirent->d_type = IFTODT(inodes[dentry->d_inode].i_mode);
    strncpy(dirent->d_name, dentry->d_name, NAME_MAX);

    /* Update the dentry pointer */
    dirp->dentry_list = dirp->dentry_list->next;

    return 0;
}

/* Given a block index and return the address of that block */
uint32_t fs_get_block_addr(struct inode *inode, int blk_index)
{
    /* Load the device file */
    struct file *dev_file = mount_points[inode->i_rdev].dev_file;

    uint32_t blk_addr = (uint32_t) NULL;
    struct block_header blk_head = {
        .b_next = inode->i_data,
    }; /* The first block address = inode->i_data */

    int i;
    for (i = 0; i < (blk_index + 1); i++) {
        /* Get the address of the next block */
        blk_addr = blk_head.b_next;

        /* Read the block header */
        dev_file->f_op->read(NULL, (char *) &blk_head,
                             sizeof(struct block_header), blk_addr);
    }

    return blk_addr;
}

/* Max size of the arguments a file system request can carry, which is two
 * pointers at the moment
 */
#define FS_ARGS_SIZE_MAX 8

/* Send a request to the file system daemon. The reply is read back from the
 * anonymous pipe of the calling thread by the VFS layer.
 */
static void fs_request(int fs_cmd,
                       int reply_fd,
                       const void *args,
                       size_t args_size)
{
    preempt_disable();

    char buf[sizeof(fs_cmd) + sizeof(reply_fd) + FS_ARGS_SIZE_MAX];
    int buf_size = 0;

    memcpy(&buf[buf_size], &fs_cmd, sizeof(fs_cmd));
    buf_size += sizeof(fs_cmd);

    memcpy(&buf[buf_size], &reply_fd, sizeof(reply_fd));
    buf_size += sizeof(reply_fd);

    memcpy(&buf[buf_size], args, args_size);
    buf_size += args_size;

    const int filesysd_fd = THREAD_PIPE_FD(get_daemon_id(FILESYSD));
    fifo_write(files[filesysd_fd], buf, buf_size, 0);

    preempt_enable();
}

void request_rename(int thread_id, const char *oldpath, const char *newpath)
{
    char args[sizeof(oldpath) + sizeof(newpath)];

    memcpy(&args[0], &oldpath, sizeof(oldpath));
    memcpy(&args[sizeof(oldpath)], &newpath, sizeof(newpath));

    fs_request(FS_RENAME, THREAD_PIPE_FD(thread_id), args, sizeof(args));
}

void request_stat(int thread_id, const char *path, struct stat *statbuf)
{
    char args[sizeof(path) + sizeof(statbuf)];

    memcpy(&args[0], &path, sizeof(path));
    memcpy(&args[sizeof(path)], &statbuf, sizeof(statbuf));

    fs_request(FS_STAT, THREAD_PIPE_FD(thread_id), args, sizeof(args));
}

void request_remove(int thread_id, const char *path, bool rm_dir)
{
    char args[sizeof(path) + sizeof(rm_dir)];

    memcpy(&args[0], &path, sizeof(path));
    memcpy(&args[sizeof(path)], &rm_dir, sizeof(rm_dir));

    fs_request(FS_REMOVE, THREAD_PIPE_FD(thread_id), args, sizeof(args));
}

void request_mkdir(int thread_id, const char *path)
{
    fs_request(FS_MAKE_DIR, THREAD_PIPE_FD(thread_id), &path, sizeof(path));
}

void request_create_file(int thread_id, const char *path, mode_t mode)
{
    preempt_disable();

    int fs_cmd = FS_CREATE_FILE;
    int reply_fd = THREAD_PIPE_FD(thread_id);
    const size_t overhead =
        sizeof(fs_cmd) + sizeof(reply_fd) + sizeof(path) + sizeof(mode);
    char buf[overhead];
    int buf_size = 0;

    memcpy(&buf[buf_size], &fs_cmd, sizeof(fs_cmd));
    buf_size += sizeof(fs_cmd);

    memcpy(&buf[buf_size], &reply_fd, sizeof(reply_fd));
    buf_size += sizeof(reply_fd);

    memcpy(&buf[buf_size], &path, sizeof(path));
    buf_size += sizeof(path);

    memcpy(&buf[buf_size], &mode, sizeof(mode));
    buf_size += sizeof(mode);

    const int filesysd_fd = THREAD_PIPE_FD(get_daemon_id(FILESYSD));
    fifo_write(files[filesysd_fd], buf, buf_size, 0);

    preempt_enable();
}

void request_open_file(int thread_id, const char *path)
{
    preempt_disable();

    int fs_cmd = FS_OPEN_FILE;
    int reply_fd = THREAD_PIPE_FD(thread_id);
    const size_t overhead = sizeof(fs_cmd) + sizeof(reply_fd) + sizeof(path);
    char buf[overhead];
    int buf_size = 0;

    memcpy(&buf[buf_size], &fs_cmd, sizeof(fs_cmd));
    buf_size += sizeof(fs_cmd);

    memcpy(&buf[buf_size], &reply_fd, sizeof(reply_fd));
    buf_size += sizeof(reply_fd);

    memcpy(&buf[buf_size], &path, sizeof(path));
    buf_size += sizeof(path);

    const int filesysd_fd = THREAD_PIPE_FD(get_daemon_id(FILESYSD));
    fifo_write(files[filesysd_fd], buf, buf_size, 0);

    preempt_enable();
}

void request_open_directory(int thread_id, const char *path)
{
    preempt_disable();

    int fs_cmd = FS_OPEN_DIR;
    int reply_fd = THREAD_PIPE_FD(thread_id);
    const size_t overhead = sizeof(fs_cmd) + sizeof(reply_fd) + sizeof(path);
    char buf[overhead];
    int buf_size = 0;

    memcpy(&buf[buf_size], &fs_cmd, sizeof(fs_cmd));
    buf_size += sizeof(fs_cmd);

    memcpy(&buf[buf_size], &reply_fd, sizeof(reply_fd));
    buf_size += sizeof(reply_fd);

    memcpy(&buf[buf_size], &path, sizeof(path));
    buf_size += sizeof(path);

    const int filesysd_fd = THREAD_PIPE_FD(get_daemon_id(FILESYSD));
    fifo_write(files[filesysd_fd], buf, buf_size, 0);

    preempt_enable();
}

void request_mount(int thread_id, const char *source, const char *target)
{
    preempt_disable();

    int fs_cmd = FS_MOUNT;
    int reply_fd = THREAD_PIPE_FD(thread_id);
    const size_t overhead = sizeof(fs_cmd) + sizeof(reply_fd) +
                            sizeof(sizeof(source)) + sizeof(sizeof(target));
    char buf[overhead];
    int buf_size = 0;

    memcpy(&buf[buf_size], &fs_cmd, sizeof(fs_cmd));
    buf_size += sizeof(fs_cmd);

    memcpy(&buf[buf_size], &reply_fd, sizeof(reply_fd));
    buf_size += sizeof(reply_fd);

    memcpy(&buf[buf_size], &source, sizeof(source));
    buf_size += sizeof(source);

    memcpy(&buf[buf_size], &target, sizeof(target));
    buf_size += sizeof(target);

    const int filesysd_fd = THREAD_PIPE_FD(get_daemon_id(FILESYSD));
    fifo_write(files[filesysd_fd], buf, buf_size, 0);

    preempt_enable();
}

void request_getcwd(int thread_id, char *path, size_t len)
{
    preempt_disable();

    int fs_cmd = FS_GET_CWD;
    int reply_fd = THREAD_PIPE_FD(thread_id);
    const size_t overhead =
        sizeof(fs_cmd) + sizeof(path) + sizeof(len) + sizeof(reply_fd);
    char buf[overhead];
    int buf_size = 0;

    memcpy(&buf[buf_size], &fs_cmd, sizeof(fs_cmd));
    buf_size += sizeof(fs_cmd);

    memcpy(&buf[buf_size], &reply_fd, sizeof(reply_fd));
    buf_size += sizeof(reply_fd);

    memcpy(&buf[buf_size], &path, sizeof(path));
    buf_size += sizeof(path);

    memcpy(&buf[buf_size], &len, sizeof(len));
    buf_size += sizeof(len);

    const int filesysd_fd = THREAD_PIPE_FD(get_daemon_id(FILESYSD));
    fifo_write(files[filesysd_fd], buf, buf_size, 0);

    preempt_enable();
}

void request_chdir(int thread_id, const char *path)
{
    preempt_disable();

    int fs_cmd = FS_CHANGE_DIR;
    int reply_fd = THREAD_PIPE_FD(thread_id);
    const size_t overhead = sizeof(fs_cmd) + sizeof(path) + sizeof(reply_fd);
    char buf[overhead];
    int buf_size = 0;

    memcpy(&buf[buf_size], &fs_cmd, sizeof(fs_cmd));
    buf_size += sizeof(fs_cmd);

    memcpy(&buf[buf_size], &reply_fd, sizeof(reply_fd));
    buf_size += sizeof(reply_fd);

    memcpy(&buf[buf_size], &path, sizeof(path));
    buf_size += sizeof(path);

    const int filesysd_fd = THREAD_PIPE_FD(get_daemon_id(FILESYSD));
    fifo_write(files[filesysd_fd], buf, buf_size, 0);

    preempt_enable();
}

void filesysd(void)
{
    setprogname("filesysd");
    set_daemon_id(FILESYSD);

    const int filesysd_fd = THREAD_PIPE_FD(get_daemon_id(FILESYSD));

    while (1) {
        int file_cmd;
        read(filesysd_fd, &file_cmd, sizeof(file_cmd));

        int reply_fd;
        read(filesysd_fd, &reply_fd, sizeof(reply_fd));

        switch (file_cmd) {
        case FS_CREATE_FILE: {
            char *path;
            read(filesysd_fd, &path, sizeof(path));

            mode_t mode;
            read(filesysd_fd, &mode, sizeof(mode));

            int new_fd = fs_create_file(path, mode, false);
            write(reply_fd, &new_fd, sizeof(new_fd));

            break;
        }
        case FS_OPEN_FILE: {
            char *path;
            read(filesysd_fd, &path, sizeof(path));

            int open_fd = fs_open_file(path);
            write(reply_fd, &open_fd, sizeof(open_fd));

            break;
        }
        case FS_OPEN_DIR: {
            char *path;
            read(filesysd_fd, &path, sizeof(path));

            struct inode *inode_dir = fs_open_directory(path);

            write(reply_fd, &inode_dir, sizeof(inode_dir));

            break;
        }
        case FS_MOUNT: {
            char *source;
            read(filesysd_fd, &source, sizeof(source));

            char *target;
            read(filesysd_fd, &target, sizeof(target));

            int result = fs_mount(source, target);
            write(reply_fd, &result, sizeof(result));

            break;
        }
        case FS_GET_CWD: {
            char *buf;
            read(filesysd_fd, &buf, sizeof(buf));

            size_t len;
            read(filesysd_fd, &len, sizeof(len));

            char *retval = fs_getcwd(buf, len);
            write(reply_fd, &retval, sizeof(retval));

            break;
        }
        case FS_CHANGE_DIR: {
            char *path;
            read(filesysd_fd, &path, sizeof(path));

            int result = fs_chdir(path);
            write(reply_fd, &result, sizeof(result));

            break;
        }
        case FS_MAKE_DIR: {
            char *path;
            read(filesysd_fd, &path, sizeof(path));

            int result = fs_mkdir(path);
            write(reply_fd, &result, sizeof(result));

            break;
        }
        case FS_REMOVE: {
            char *path;
            read(filesysd_fd, &path, sizeof(path));

            bool rm_dir;
            read(filesysd_fd, &rm_dir, sizeof(rm_dir));

            int result = fs_remove(path, rm_dir);
            write(reply_fd, &result, sizeof(result));

            break;
        }
        case FS_STAT: {
            char *path;
            read(filesysd_fd, &path, sizeof(path));

            struct stat *statbuf;
            read(filesysd_fd, &statbuf, sizeof(statbuf));

            int result = fs_stat(path, statbuf);
            write(reply_fd, &result, sizeof(result));

            break;
        }
        case FS_RENAME: {
            char *oldpath;
            read(filesysd_fd, &oldpath, sizeof(oldpath));

            char *newpath;
            read(filesysd_fd, &newpath, sizeof(newpath));

            int result = fs_rename(oldpath, newpath);
            write(reply_fd, &result, sizeof(result));

            break;
        }
        }
    }
}

/**
 * Debug only:
 */
#include "shell.h"

void fs_print_inode_bitmap(void)
{
    char buf[100] = {0};

    shell_puts("inodes bitmap:\n\r");

    for (int i = 0; i < BITMAP_SIZE(INODE_MAX); i++) {
        for (int j = 0; j < 8; j++) {
            int bit = (bitmap_inodes[i] >> j) & (~1l);
            buf[j] = bit ? 'x' : '-';
        }
        buf[8] = '\n';
        buf[9] = '\r';
        buf[10] = '\0';

        shell_puts(buf);
    }
}

void fs_print_block_bitmap(void)
{
    char buf[100] = {0};

    shell_puts("fs blocks bitmap:\n\r");

    for (int i = 0; i < BITMAP_SIZE(FS_BLK_CNT); i++) {
        for (int j = 0; j < 8; j++) {
            int bit = (bitmap_blks[i] >> j) & (~1l);
            buf[j] = bit ? 'x' : '-';
        }
        buf[8] = '\n';
        buf[9] = '\r';
        buf[10] = '\0';

        shell_puts(buf);
    }
}
