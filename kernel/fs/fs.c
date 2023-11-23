#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <sys/resource.h>
#include <sys/stat.h>
#include <tenok.h>
#include <unistd.h>

#include <common/bitops.h>
#include <fs/fs.h>
#include <fs/reg_file.h>
#include <kernel/interrupt.h>
#include <kernel/kernel.h>
#include <kernel/pipe.h>
#include <kernel/wait.h>
#include <mm/mm.h>
#include <mm/slab.h>

#include "rom_dev.h"
#include "uart.h"

static void fs_mount_directory(struct inode *inode_src,
                               struct inode *inode_target);
static int fs_create_file(char *pathname, uint8_t file_type);
static int fs_open_file(char *pathname);
ssize_t rootfs_read(struct file *filp, char *buf, size_t size, off_t offset);
ssize_t rootfs_write(struct file *filp,
                     const char *buf,
                     size_t size,
                     off_t offset);

extern struct file *files[FILE_RESERVED_NUM + FILE_CNT_MAX];
extern int file_cnt;

struct inode inodes[INODE_CNT_MAX];
uint8_t rootfs_blks[FS_BLK_CNT][FS_BLK_SIZE];

uint32_t bitmap_inodes[FS_BITMAP_INODE] = {0};
uint32_t bitmap_blks[FS_BITMAP_BLK] = {0};

struct mount mount_points[MOUNT_CNT_MAX + 1]; /* 0 is reserved for the rootfs */
int mount_cnt = 0;

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
    while (fd < 0)
        ;
    files[STDIN_FILENO] = files[fd];
}

void link_stdout_dev(char *path)
{
    int fd = fs_open_file(path);
    while (fd < 0)
        ;
    files[STDOUT_FILENO] = files[fd];
}

void link_stderr_dev(char *path)
{
    int fd = fs_open_file(path);
    while (fd < 0)
        ;
    files[STDERR_FILENO] = files[fd];
}

int register_chrdev(char *name, struct file_operations *fops)
{
    char dev_path[100] = {0};
    snprintf(dev_path, PATH_LEN_MAX, "/dev/%s", name);

    /* Create new character device file */
    int fd = fs_create_file(dev_path, S_IFCHR);

    /* Link the file operations */
    files[fd]->f_op = fops;

    return 0;
}

int register_blkdev(char *name, struct file_operations *fops)
{
    char dev_path[100] = {0};
    snprintf(dev_path, PATH_LEN_MAX, "/dev/%s", name);

    /* Create new block device file */
    int fd = fs_create_file(dev_path, S_IFBLK);

    /* Link the file operations */
    files[fd]->f_op = fops;

    return 0;
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
    int free_idx = find_first_zero_bit(bitmap_inodes, INODE_CNT_MAX);
    if (free_idx < INODE_CNT_MAX) {
        /* Allocate new inode */
        new_inode = &inodes[free_idx];
        new_inode->i_ino = free_idx;
        mount_points[RDEV_ROOTFS].super_blk.s_inode_cnt++;

        /* Update the inode bitmap */
        bitmap_set_bit(bitmap_inodes, free_idx);
    }

    return new_inode;
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
    inode_root->i_mode = S_IFDIR;
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

    int fd = file_cnt + FILE_RESERVED_NUM;
    files[fd] = rootfs_file;
    file_cnt++;

    /* Mount the rootfs */
    mount_points[RDEV_ROOTFS].dev_file = rootfs_file;
    mount_cnt = 1;
}

static bool rootfs_mem_check(uint32_t addr)
{
    bool pass = false;

    uint32_t inode_start_addr = (uint32_t) inodes;
    uint32_t inode_end_addr =
        (uint32_t) inodes + (sizeof(struct inode) * INODE_CNT_MAX);

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
static struct inode *fs_search_file(struct inode *inode_dir, char *file_name)
{
    /* The dentry table is currently empty */
    if (inode_dir->i_size == 0)
        return NULL;

    /* Return current inode */
    if (strncmp(".", file_name, FILE_NAME_LEN_MAX) == 0)
        return inode_dir;

    /* Return parent inode */
    if (strncmp("..", file_name, FILE_NAME_LEN_MAX) == 0)
        return &inodes[inode_dir->i_parent];

    /* Mount directory to synchronize it */
    if (inode_dir->i_sync == false)
        fs_mount_directory(inode_dir, inode_dir);

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

static struct dentry *fs_allocate_dentry(struct inode *inode_dir)
{
    /* Calculate how many dentries a block can hold */
    int dentry_per_blk = FS_BLK_SIZE / sizeof(struct dentry);

    /* Calculate how many dentries the directory has */
    int dentry_cnt = inode_dir->i_size / sizeof(struct dentry);

    /* Check if current block can fit a new dentry */
    bool fit =
        ((dentry_cnt + 1) <= (inode_dir->i_blocks * dentry_per_blk)) &&
        (inode_dir->i_size != 0) /* No memory is allocated if size = 0 */;

    /* Allocate new dentry */
    uint8_t *dir_data_p;
    if (fit == true) {
        /* Append at the end of the old block */
        struct list_head *list_end = inode_dir->i_dentry.prev;
        struct dentry *dir = list_entry(list_end, struct dentry, d_list);
        dir_data_p = (uint8_t *) dir + sizeof(struct dentry);
    } else {
        /* The dentry requires a new block */
        dir_data_p = fs_alloc_block();
    }

    return (struct dentry *) dir_data_p;
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

/* Create a file under the given directory (currently only supports rootfs) */
static struct inode *fs_add_file(struct inode *inode_dir,
                                 char *file_name,
                                 int file_type)
{
    /* inodes table is full */
    if (mount_points[0].super_blk.s_inode_cnt >= INODE_CNT_MAX)
        goto failed;

    /* File table is full */
    if (file_cnt >= FILE_CNT_MAX)
        goto failed;

    /* Dispatch new file descriptor number */
    int fd = file_cnt + FILE_RESERVED_NUM;

    /* Allocate new inode for the file */
    struct inode *new_inode = fs_alloc_inode();
    new_inode->i_rdev = RDEV_ROOTFS;
    new_inode->i_parent = inode_dir->i_ino;
    new_inode->i_fd = fd;
    new_inode->i_sync = true;

    /* Configure new dentry */
    struct dentry *new_dentry = fs_allocate_dentry(inode_dir);
    new_dentry->d_inode = new_inode->i_ino;  /* file inode */
    new_dentry->d_parent = inode_dir->i_ino; /* parent inode */
    strncpy(new_dentry->d_name, file_name, FILE_NAME_LEN_MAX); /* file name */

    /* File instantiation */
    int result = 0;

    switch (file_type) {
    case S_IFIFO: {
        /* Named pipe */
        struct pipe *pipe = kmalloc(sizeof(struct pipe));
        struct kfifo *pipe_fifo = kfifo_alloc(1, PIPE_DEPTH);

        /* Allocation failure */
        if (!pipe || !pipe_fifo)
            goto failed;

        pipe->fifo = pipe_fifo;
        result = fifo_init(fd, (struct file **) &files, new_inode, pipe);

        new_inode->i_mode = S_IFIFO;
        new_inode->i_size = 0;
        new_inode->i_blocks = 0;
        new_inode->i_data = (uint32_t) NULL;

        break;
    }
    case S_IFCHR: {
        /* Character device */
        struct file *dev_file = fs_alloc_file();

        /* Allocation failure */
        if (!dev_file)
            goto failed;

        dev_file->f_inode = new_inode;
        files[fd] = dev_file;

        new_inode->i_mode = S_IFCHR;
        new_inode->i_size = 0;
        new_inode->i_blocks = 0;
        new_inode->i_data = (uint32_t) NULL;

        break;
    }
    case S_IFBLK: {
        /* Block device */
        struct file *dev_file = fs_alloc_file();

        /* Allocation failure */
        if (!dev_file)
            goto failed;

        dev_file->f_inode = new_inode;
        files[fd] = dev_file;

        new_inode->i_mode = S_IFBLK;
        new_inode->i_size = 0;
        new_inode->i_blocks = 0;
        new_inode->i_data = (uint32_t) NULL;

        break;
    }
    case S_IFREG: {
        /* Regular file */
        struct reg_file *reg_file = kmalloc(sizeof(struct reg_file));

        /* Allocation failure */
        if (!reg_file)
            goto failed;

        result = reg_file_init((struct file **) &files, new_inode, reg_file);

        new_inode->i_mode = S_IFREG;
        new_inode->i_size = 0;
        new_inode->i_blocks = 0;
        new_inode->i_data = (uint32_t) NULL; /* Empty content */

        break;
    }
    case S_IFDIR:
        /* Directory */
        new_inode->i_mode = S_IFDIR;
        new_inode->i_size = 0;
        new_inode->i_blocks = 0;
        new_inode->i_data = (uint32_t) NULL; /* Empty directory */
        INIT_LIST_HEAD(&new_inode->i_dentry);

        break;
    default:
        result = -1;
    }

    /* Initialize file events */
    files[fd]->f_events = 0;

    if (result != 0)
        goto failed;

    /* Update file count */
    file_cnt++;

    /* Currently the directory has no file yet */
    if (list_empty(&inode_dir->i_dentry) == true)
        inode_dir->i_data = (uint32_t) new_dentry; /* Add first dentry */

    /* insert the new file under the directory */
    list_add(&new_dentry->d_list, &inode_dir->i_dentry);

    /* Update the inode size and block information */
    inode_dir->i_size += sizeof(struct dentry);

    int dentry_cnt = inode_dir->i_size / sizeof(struct dentry);
    inode_dir->i_blocks = fs_calculate_dentry_blocks(FS_BLK_SIZE, dentry_cnt);

    return new_inode;

failed:
    // TODO: Clean up the allocated resources
    return NULL;
}

/* Must be called by the fs_mount_directory() function since current design only
 * supports mounting a whole directory */
static struct inode *fs_mount_file(struct inode *inode_dir,
                                   struct inode *mnt_inode,
                                   struct dentry *mnt_dentry)
{
    /* inodes table is full */
    if (mount_points[0].super_blk.s_inode_cnt >= INODE_CNT_MAX)
        return NULL;

    /* Dispatch new file descriptor number */
    int fd = file_cnt + FILE_RESERVED_NUM;
    file_cnt++;

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
    struct dentry *new_dentry = fs_allocate_dentry(inode_dir);
    new_dentry->d_inode = new_inode->i_ino;  /* File inode */
    new_dentry->d_parent = inode_dir->i_ino; /* Parent inode */
    strncpy(new_dentry->d_name, mnt_dentry->d_name,
            FILE_NAME_LEN_MAX); /* File name */

    /* Update inode count */
    mount_points[RDEV_ROOTFS].super_blk.s_inode_cnt++;

    /* Insert the new file under current directory */
    list_add(&new_dentry->d_list, &inode_dir->i_dentry);

    /* Update the inode size and block information */
    inode_dir->i_size += sizeof(struct dentry);

    int dentry_cnt = inode_dir->i_size / sizeof(struct dentry);
    inode_dir->i_blocks = fs_calculate_dentry_blocks(FS_BLK_SIZE, dentry_cnt);

    return new_inode;
}

static bool fs_sync_file(struct inode *inode)
{
    /* rootfs files does not require synchronization */
    if (inode->i_rdev == RDEV_ROOTFS)
        return false;

    /* Only regular files require synchronization */
    if (inode->i_mode != S_IFREG)
        return false;

    /* The file is already synchronized */
    if (inode->i_sync == true)
        return false;

    /* The file table is full */
    if (file_cnt >= FILE_CNT_MAX)
        return false;

    /* Dispatch new file descriptor number */
    inode->i_fd = file_cnt + FILE_RESERVED_NUM;
    file_cnt++;

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
        if ((uint32_t) dentry.d_list.next <
            (sb_size + inode_size * INODE_CNT_MAX))
            break; /* no more dentry to read */
    }

    /* The directory is now synchronized */
    inode_target->i_sync = true;
}

/* Get the first entry of a given path. For example, given the pathname
 * "dir/file1.txt", the function should return "dir".
 * Input : Pathname
 * Output: Entry name and the reduced path string
 */
static char *fs_split_path(char *entry, char *path)
{
    while (1) {
        bool found_dir = (*path == '/');

        /* Copy */
        if (found_dir == false) {
            *entry = *path;
            entry++;
        }

        path++;

        if ((found_dir == true) || (*path == '\0'))
            break;
    }

    *entry = '\0';

    /* End of the string */
    if (*path == '\0')
        return NULL; /* No more string to split */

    /* Return the address of the left path string */
    return path;
}

/* Create a file by given the pathname
 * Input : Path name and file type
 * Output: File descriptor number
 */
static int fs_create_file(char *pathname, uint8_t file_type)
{
    /* The path name must start with '/' */
    if (pathname[0] != '/')
        return -1;

    /* iterate from the root inode */
    struct inode *inode_curr = &inodes[0];
    struct inode *inode;

    char file_name[FILE_NAME_LEN_MAX];
    char entry[PATH_LEN_MAX];
    char *path = pathname;

    /* get rid of the first '/' */
    path = fs_split_path(entry, path);

    while (1) {
        /* Split the path and get the entry name of each layer */
        path = fs_split_path(entry, path);

        /* Two successive '/' are detected */
        if (entry[0] == '\0') {
            if (path == NULL) {
                return -1;
            } else {
                continue;
            }
        }

        /* The last non-empty entry is the file name */
        if (entry[0] != '\0')
            strncpy(file_name, entry, FILE_NAME_LEN_MAX);

        /* Search the entry and get the inode */
        inode = fs_search_file(inode_curr, entry);

        if (path != NULL) {
            /* The path can be further splitted, i.e., it is a directory */

            /* Check if the directory exists */
            if (inode == NULL) {
                /* Directory does not exist, create one */
                inode = fs_add_file(inode_curr, entry, S_IFDIR);

                /* Failed to create the directory */
                if (inode == NULL)
                    return -1;
            }

            inode_curr = inode;
        } else {
            /* No more path to be splitted, the remained string should be the
             * file name */

            /* File with the same name already exists */
            if (inode != NULL)
                return -1;

            /* Create new inode for the file */
            inode = fs_add_file(inode_curr, file_name, file_type);

            /* Failed to create the file */
            if (inode == NULL)
                return -1;

            /* File is created successfully */
            return inode->i_fd;
        }
    }
}

/* Open a file by given a pathname
 * Input : Pathname
 * Output: File descriptor number
 */
static int fs_open_file(char *pathname)
{
    /* The pathname must start with '/' */
    if (pathname[0] != '/')
        return -1;

    /* Iterate from the root inode */
    struct inode *inode_curr = &inodes[0];
    struct inode *inode;

    char entry[PATH_LEN_MAX] = {0};
    char *path = pathname;

    /* Get rid of the first '/' */
    path = fs_split_path(entry, path);

    while (1) {
        /* Split the path and get the entry name of each layer */
        path = fs_split_path(entry, path);

        /* Two successive '/' are detected */
        if (entry[0] == '\0') {
            if (path == NULL) {
                return -1;
            } else {
                continue;
            }
        }

        /* Search the entry and get the inode */
        inode = fs_search_file(inode_curr, entry);

        /* File or directory not found */
        if (inode == NULL)
            return -1;

        if (path != NULL) {
            /* The path can be further splitted, the iteration can get deeper */

            /* Failed, not a directory */
            if (inode->i_mode != S_IFDIR)
                return -1;

            inode_curr = inode;
        } else {
            /* No more path to be splitted, the remained string should be the
             * file name */

            /* Make sure the last char is not equal to '/' */
            int len = strlen(pathname);
            if (pathname[len - 1] == '/')
                return -1;

            /* Check if the file requires synchronization */
            if ((inode->i_rdev != RDEV_ROOTFS) && (inode->i_sync == false)) {
                /* Synchronize the file */
                fs_sync_file(inode);
            }

            /* Failed, not a file */
            if (inode->i_mode == S_IFDIR)
                return -1;

            /* File is open successfully */
            return inode->i_fd;
        }
    }
}

/* Input : File pathname
 * Output: Directory inode
 */
struct inode *fs_open_directory(char *pathname)
{
    /* The path name must start with '/' */
    if (pathname[0] != '/')
        return NULL;

    /* Iterate from the root inode */
    struct inode *inode_curr = &inodes[0];
    struct inode *inode;

    char entry_curr[PATH_LEN_MAX] = {0};
    char *path = pathname;

    /* Get rid of the first '/' */
    path = fs_split_path(entry_curr, path);

    if (path == NULL)
        return inode_curr;

    while (1) {
        /* Split the path and get the entry hierarchically */
        path = fs_split_path(entry_curr, path);

        /* Two successive '/' are detected */
        if (entry_curr[0] == '\0') {
            if (path == NULL) {
                break;
            } else {
                continue;
            }
        }

        /* Search the entry and get the inode */
        inode = fs_search_file(inode_curr, entry_curr);

        /* Directory does not exist */
        if (inode == NULL)
            return NULL;

        /* Not a directory */
        if (inode->i_mode != S_IFDIR)
            return NULL;

        inode_curr = inode;

        /* No more sub-string to be splitted */
        if (path == NULL)
            break;
    }

    /* Synchronize the directory */
    if (inode_curr->i_sync == false)
        fs_mount_directory(inode_curr, inode_curr);

    return inode_curr;
}

static int fs_mount(char *source, char *target)
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
    char old_path[PATH_LEN_MAX] = {0};
    char new_path[PATH_LEN_MAX] = {0};

    struct inode *inode = shell_dir_curr;

    while (1) {
        if (inode->i_ino == 0)
            break;

        /* Switch to the parent directory */
        uint32_t inode_prev = inode->i_ino;
        inode = &inodes[inode->i_parent];

        /* No file is under this directory */
        if (list_empty(&inode->i_dentry) == true)
            break;

        struct dentry *dentry;
        list_for_each_entry (dentry, &inode->i_dentry, d_list) {
            if (dentry->d_inode == inode_prev) {
                strncpy(old_path, new_path, PATH_LEN_MAX);
                snprintf(new_path, PATH_LEN_MAX, "%s/%s", dentry->d_name,
                         old_path);
                break;
            }
        }
    }

    snprintf(buf, len, "/%s", new_path);

    return buf;
}

int fs_chdir(const char *path)
{
    char path_tmp[PATH_LEN_MAX] = {0};

    if (strncmp("..", path, SHELL_CMD_LEN_MAX) == 0) {
        /* Handle cd .. */
        fs_getcwd(path_tmp, PATH_LEN_MAX);
        int pos = strlen(path_tmp);
        snprintf(&path_tmp[pos], PATH_LEN_MAX, "..");
    } else {
        /* Handle regular path */
        if (path[0] == '/') {
            /* Handle absolute path */
            strncpy(path_tmp, path, PATH_LEN_MAX);
        } else {
            /* Handle relative path */
            fs_getcwd(path_tmp, PATH_LEN_MAX);
            int pos = strlen(path_tmp);
            snprintf(&path_tmp[pos], PATH_LEN_MAX, "%s", path);
        }
    }

    /* Open the directory */
    struct inode *inode_dir = fs_open_directory(path_tmp);

    /* Directory not found */
    if (!inode_dir) {
        return -ENOENT;
    }

    /* Not a directory */
    if (inode_dir->i_mode != S_IFDIR) {
        return -ENOTDIR;
    }

    shell_dir_curr = inode_dir;

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
    dirent->d_type = inodes[dentry->d_inode].i_mode;
    strncpy(dirent->d_name, dentry->d_name, FILE_NAME_LEN_MAX);

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

void request_create_file(int thread_id, const char *path, uint8_t file_type)
{
    int fs_cmd = FS_CREATE_FILE;
    int reply_fd = THREAD_PIPE_FD(thread_id);
    const size_t overhead =
        sizeof(fs_cmd) + sizeof(reply_fd) + sizeof(path) + sizeof(file_type);
    char buf[overhead];
    int buf_size = 0;

    memcpy(&buf[buf_size], &fs_cmd, sizeof(fs_cmd));
    buf_size += sizeof(fs_cmd);

    memcpy(&buf[buf_size], &reply_fd, sizeof(reply_fd));
    buf_size += sizeof(reply_fd);

    memcpy(&buf[buf_size], &path, sizeof(path));
    buf_size += sizeof(path);

    memcpy(&buf[buf_size], &file_type, sizeof(file_type));
    buf_size += sizeof(file_type);

    const int filesysd_fd = THREAD_PIPE_FD(get_daemon_id(FILESYSD));
    fifo_write(files[filesysd_fd], buf, buf_size, 0);
}

void request_open_file(int thread_id, const char *path)
{
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
}

void request_open_directory(int thread_id, const char *path)
{
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
}

void request_mount(int thread_id, const char *source, const char *target)
{
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
}

void request_getcwd(int thread_id, char *path, size_t len)
{
    int fs_cmd = FS_GET_CWD;
    int reply_fd = THREAD_PIPE_FD(thread_id);
    const size_t overhead = sizeof(path) + sizeof(len) + sizeof(reply_fd);
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
}

void request_chdir(int thread_id, const char *path)
{
    int fs_cmd = FS_CHANGE_DIR;
    int reply_fd = THREAD_PIPE_FD(thread_id);
    const size_t overhead = sizeof(path) + sizeof(reply_fd);
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

            uint8_t file_type;
            read(filesysd_fd, &file_type, sizeof(file_type));

            int new_fd = fs_create_file(path, file_type);
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

    for (int i = 0; i < INODE_CNT_MAX; i++) {
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

    for (int i = 0; i < FS_BITMAP_BLK; i++) {
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
