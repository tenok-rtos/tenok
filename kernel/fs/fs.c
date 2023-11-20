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

ssize_t rootfs_read(struct file *filp, char *buf, size_t size, off_t offset);
ssize_t rootfs_write(struct file *filp,
                     const char *buf,
                     size_t size,
                     off_t offset);

extern struct file *files[TASK_CNT_MAX + FILE_CNT_MAX];
extern int file_cnt;

struct inode inodes[INODE_CNT_MAX];
uint8_t rootfs_blks[FS_BLK_CNT][FS_BLK_SIZE];

uint32_t bitmap_inodes[FS_BITMAP_INODE] = {0};
uint32_t bitmap_blks[FS_BITMAP_BLK] = {0};

struct mount mount_points[MOUNT_CNT_MAX + 1];  // 0 is reserved for the rootfs
int mount_cnt = 0;

struct inode *shell_dir_curr;

struct kmem_cache *file_caches;

static struct file_operations rootfs_file_ops = {
    .read = rootfs_read,
    .write = rootfs_write,
};

int register_chrdev(char *name, struct file_operations *fops)
{
    char dev_path[100] = {0};
    snprintf(dev_path, PATH_LEN_MAX, "/dev/%s", name);

    /* create new character device file */
    int fd = fs_create_file(dev_path, S_IFCHR);

    /* link the file operations */
    files[fd]->f_op = fops;

    return 0;
}

int register_blkdev(char *name, struct file_operations *fops)
{
    char dev_path[100] = {0};
    snprintf(dev_path, PATH_LEN_MAX, "/dev/%s", name);

    /* create new block device file */
    int fd = fs_create_file(dev_path, S_IFBLK);

    /* link the file operations */
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

    /* find the first free inode */
    int free_idx = find_first_zero_bit(bitmap_inodes, INODE_CNT_MAX);
    if (free_idx < INODE_CNT_MAX) {
        /* allocate a new inode */
        new_inode = &inodes[free_idx];
        new_inode->i_ino = free_idx;
        mount_points[RDEV_ROOTFS].super_blk.s_inode_cnt++;

        /* update the inode bitmap */
        bitmap_set_bit(bitmap_inodes, free_idx);
    }

    return new_inode;
}

static uint8_t *fs_alloc_block(void)
{
    uint8_t *new_block = NULL;

    /* find the first free block */
    int free_idx = find_first_zero_bit(bitmap_blks, FS_BLK_CNT);
    if (free_idx < FS_BLK_CNT) {
        /* allocate a new block */
        new_block = (uint8_t *) &rootfs_blks[free_idx];
        mount_points[RDEV_ROOTFS].super_blk.s_blk_cnt++;

        /* update the inode bitmap */
        bitmap_set_bit(bitmap_blks, free_idx);
    }

    return new_block;
}

void rootfs_init(void)
{
    file_caches = kmem_cache_create("file_cache", sizeof(struct file),
                                    sizeof(uint32_t), 0, NULL);

    /* configure the rootfs super block */
    struct super_block *rootfs_super_blk = &mount_points[RDEV_ROOTFS].super_blk;
    rootfs_super_blk->s_inode_cnt = 0;
    rootfs_super_blk->s_blk_cnt = 0;
    rootfs_super_blk->s_rd_only = false;
    rootfs_super_blk->s_sb_addr = (uint32_t) rootfs_super_blk;
    rootfs_super_blk->s_ino_addr = (uint32_t) inodes;
    rootfs_super_blk->s_blk_addr = (uint32_t) rootfs_blks;

    /* configure the root directory inode of rootfs */
    struct inode *inode_root = fs_alloc_inode();
    inode_root->i_mode = S_IFDIR;
    inode_root->i_rdev = RDEV_ROOTFS;
    inode_root->i_sync = true;
    inode_root->i_size = 0;
    inode_root->i_blocks = 0;
    inode_root->i_data = (uint32_t) NULL;
    INIT_LIST_HEAD(&inode_root->i_dentry);

    shell_dir_curr = inode_root;

    /* create new file for the rootfs */
    struct file *rootfs_file = fs_alloc_file();
    rootfs_file->f_op = &rootfs_file_ops;

    int fd = file_cnt + TASK_CNT_MAX;
    files[fd] = rootfs_file;
    file_cnt++;

    /* mount the rootfs */
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
    uint8_t *read_addr =
        (uint8_t *) offset;  // offset is the read address of the data

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
    uint8_t *write_addr =
        (uint8_t *) offset;  // offset is the read address of the data

    if (rootfs_mem_check((uint32_t) write_addr) == false)
        return -EFAULT;

    memcpy(write_addr, buf, size);

    return size;
}

static void fs_read_list(struct file *dev_file,
                         uint32_t list_addr,
                         struct list_head *list)
{
    /* read the list */
    dev_file->f_op->read(dev_file, (char *) list, sizeof(struct list_head),
                         list_addr);
}

static void fs_read_dentry(struct file *dev_file,
                           uint32_t dentry_addr,
                           struct dentry *dentry)
{
    /* read the dentry */
    dev_file->f_op->read(dev_file, (char *) dentry, sizeof(struct dentry),
                         dentry_addr);
}

static void fs_read_inode(uint8_t rdev,
                          struct file *dev_file,
                          uint32_t inode_num,
                          struct inode *inode)
{
    /* calculate the inode address */
    struct super_block *super_blk = &mount_points[rdev].super_blk;
    uint32_t inode_addr =
        super_blk->s_ino_addr + (sizeof(struct inode) * inode_num);

    /* read the inode */
    dev_file->f_op->read(dev_file, (char *) inode, sizeof(struct inode),
                         inode_addr);
}

/* search a file under the given directory
 * input : directory inode, file name
 * output: file inode
 */
static struct inode *fs_search_file(struct inode *inode_dir, char *file_name)
{
    /* currently the dentry table is empty */
    if (inode_dir->i_size == 0)
        return NULL;

    /* return the current inode */
    if (strncmp(".", file_name, FILE_NAME_LEN_MAX) == 0)
        return inode_dir;

    /* return the parent inode */
    if (strncmp("..", file_name, FILE_NAME_LEN_MAX) == 0)
        return &inodes[inode_dir->i_parent];

    /* mount to synchronize the current directory */
    if (inode_dir->i_sync == false)
        fs_mount_directory(inode_dir, inode_dir);

    /* traversal of the dentry list */
    struct dentry *dentry;
    list_for_each_entry (dentry, &inode_dir->i_dentry, d_list) {
        /* compare the file name with the dentry */
        if (strcmp(dentry->d_name, file_name) == 0)
            return &inodes[dentry->d_inode];
    }

    return NULL;
}

static int fs_calculate_dentry_blocks(size_t block_size, size_t dentry_cnt)
{
    /* calculate how many dentries can a block hold */
    int dentry_per_blk = block_size / sizeof(struct dentry);

    /* calculate how many blocks is required for storing N dentries */
    int blocks = dentry_cnt / dentry_per_blk;
    if (dentry_cnt % dentry_per_blk)
        blocks++;

    return blocks;
}

static struct dentry *fs_allocate_dentry(struct inode *inode_dir)
{
    /* calculate how many dentries can a block hold */
    int dentry_per_blk = FS_BLK_SIZE / sizeof(struct dentry);

    /* calculate how many dentries the directory has */
    int dentry_cnt = inode_dir->i_size / sizeof(struct dentry);

    /* check if the current block can fit a new dentry */
    bool fit =
        ((dentry_cnt + 1) <= (inode_dir->i_blocks * dentry_per_blk)) &&
        (inode_dir->i_size != 0) /* no memory is allocated if size = 0 */;

    /* allocate memory for the new dentry */
    uint8_t *dir_data_p;
    if (fit == true) {
        struct list_head *list_end = inode_dir->i_dentry.prev;
        struct dentry *dir = list_entry(list_end, struct dentry, d_list);
        dir_data_p = (uint8_t *) dir + sizeof(struct dentry);
    } else {
        /* the data requires a new block */
        dir_data_p = fs_alloc_block();
    }

    return (struct dentry *) dir_data_p;
}

uint32_t fs_file_append_block(struct inode *inode)
{
    uint8_t rdev = inode->i_rdev;

    /* this function currently only supports rootfs */
    if (rdev != RDEV_ROOTFS) {
        return (uint32_t) NULL;
    }

    /* exceeded the maximum block count */
    if (mount_points[rdev].super_blk.s_blk_cnt >= FS_BLK_CNT) {
        return (uint32_t) NULL;
    }

    /* allocate a new block */

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

    /* iterate to the last block of the file */
    blk_head = (struct block_header *) inode->i_data;
    for (int i = 1; i < inode->i_blocks; i++) {
        blk_head = (struct block_header *) blk_head->b_next;
    }

    /* append new block */
    blk_head->b_next = new_blk;
    inode->i_blocks++;

    return new_blk;
}

/* create a file under a directory (currently support for rootfs only) */
static struct inode *fs_add_file(struct inode *inode_dir,
                                 char *file_name,
                                 int file_type)
{
    /* inodes numbers is full */
    if (mount_points[0].super_blk.s_inode_cnt >= INODE_CNT_MAX)
        goto failed;

    /* file table is full */
    if (file_cnt >= FILE_CNT_MAX)
        goto failed;

    /* dispatch a new file descriptor number */
    int fd = file_cnt + TASK_CNT_MAX;

    /* configure the new file inode */
    struct inode *new_inode = fs_alloc_inode();
    new_inode->i_rdev = RDEV_ROOTFS;
    new_inode->i_parent = inode_dir->i_ino;
    new_inode->i_fd = fd;
    new_inode->i_sync = true;

    /* configure the new dentry */
    struct dentry *new_dentry = fs_allocate_dentry(inode_dir);
    new_dentry->d_inode = new_inode->i_ino;   // inode of the file
    new_dentry->d_parent = inode_dir->i_ino;  // inode of the parent file
    strncpy(new_dentry->d_name, file_name, FILE_NAME_LEN_MAX);  // file name

    /* file instantiation */
    int result = 0;

    switch (file_type) {
    case S_IFIFO: {
        /* create new named pipe */
        struct pipe *pipe = kmalloc(sizeof(struct pipe));
        struct kfifo *pipe_fifo = kfifo_alloc(1, PIPE_DEPTH);

        /* allocation failure */
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
        /* create a new character device file */
        struct file *dev_file = fs_alloc_file();

        /* allocation failure */
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
        /* create a new block device file */
        struct file *dev_file = fs_alloc_file();

        /* allocation failure */
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
        /* create a new regular file */
        struct reg_file *reg_file = kmalloc(sizeof(struct reg_file));

        /* allocation failure */
        if (!reg_file)
            goto failed;

        result = reg_file_init((struct file **) &files, new_inode, reg_file);

        new_inode->i_mode = S_IFREG;
        new_inode->i_size = 0;
        new_inode->i_blocks = 0;
        new_inode->i_data = (uint32_t) NULL;  // new file without any content

        break;
    }
    case S_IFDIR:
        /* create a new directory */
        new_inode->i_mode = S_IFDIR;
        new_inode->i_size = 0;
        new_inode->i_blocks = 0;
        new_inode->i_data = (uint32_t) NULL;  // new directory without any files
        INIT_LIST_HEAD(&new_inode->i_dentry);

        break;
    default:
        result = -1;
    }

    /* initialize file events */
    files[fd]->f_events = 0;

    if (result != 0)
        goto failed;

    /* update file count */
    file_cnt++;

    /* currently no files is under the directory */
    if (list_empty(&inode_dir->i_dentry) == true)
        inode_dir->i_data = (uint32_t) new_dentry;  // add the first dentry

    /* insert the new file under the current directory */
    list_add(&new_dentry->d_list, &inode_dir->i_dentry);

    /* updatei the size and block information of the inode */
    inode_dir->i_size += sizeof(struct dentry);

    int dentry_cnt = inode_dir->i_size / sizeof(struct dentry);
    inode_dir->i_blocks = fs_calculate_dentry_blocks(FS_BLK_SIZE, dentry_cnt);

    return new_inode;

failed:
    // TODO: clean up allocated resources
    return NULL;
}

/* must be called by the fs_mount_directory() function since current design only
 * supports mounting a whole directory */
static struct inode *fs_mount_file(struct inode *inode_dir,
                                   struct inode *mnt_inode,
                                   struct dentry *mnt_dentry)
{
    /* inodes numbers is full */
    if (mount_points[0].super_blk.s_inode_cnt >= INODE_CNT_MAX)
        return NULL;

    /* dispatch a new file descriptor number */
    int fd = file_cnt + TASK_CNT_MAX;
    file_cnt++;

    /* configure the new file inode */
    struct inode *new_inode = fs_alloc_inode();
    new_inode->i_mode = mnt_inode->i_mode;
    new_inode->i_rdev = mnt_inode->i_rdev;
    new_inode->i_parent = inode_dir->i_ino;
    new_inode->i_fd = fd;
    new_inode->i_size = mnt_inode->i_size;
    new_inode->i_blocks = mnt_inode->i_blocks;
    new_inode->i_data = mnt_inode->i_data;
    new_inode->i_sync =
        false;  // the content will be synchronized only when the file is opened
    INIT_LIST_HEAD(&new_inode->i_dentry);

    /* configure the new dentry */
    struct dentry *new_dentry = fs_allocate_dentry(inode_dir);
    new_dentry->d_inode =
        new_inode->i_ino;  // assign new inode number for the file
    new_dentry->d_parent =
        inode_dir->i_ino;  // save the inode of the parent directory
    strncpy(new_dentry->d_name, mnt_dentry->d_name,
            FILE_NAME_LEN_MAX);  // copy the file name

    /* update inode number for the next file */
    mount_points[RDEV_ROOTFS].super_blk.s_inode_cnt++;

    /* insert the new file under the current directory */
    list_add(&new_dentry->d_list, &inode_dir->i_dentry);

    /* update the size and block information of the inode */
    inode_dir->i_size += sizeof(struct dentry);

    int dentry_cnt = inode_dir->i_size / sizeof(struct dentry);
    inode_dir->i_blocks = fs_calculate_dentry_blocks(FS_BLK_SIZE, dentry_cnt);

    return new_inode;
}

static bool fs_sync_file(struct inode *inode)
{
    /* rootfs files require no synchronization */
    if (inode->i_rdev == RDEV_ROOTFS)
        return false;

    /* only regular files require synchronization */
    if (inode->i_mode != S_IFREG)
        return false;

    /* the file is already synchronized */
    if (inode->i_sync == true)
        return false;

    /* file table is full */
    if (file_cnt >= FILE_CNT_MAX)
        return false;

    /* dispatch a new file descriptor number */
    inode->i_fd = file_cnt + TASK_CNT_MAX;
    file_cnt++;

    /* create a new regular file */
    struct reg_file *reg_file = kmalloc(sizeof(struct reg_file));
    int result = reg_file_init((struct file **) &files, inode, reg_file);

    /* failed to create a new regular file */
    if (result != 0)
        return false;

    /* update the synchronization flag */
    inode->i_sync = true;

    return true;
}

/* mount a directory from a external storage under the rootfs
 * input: "inode_src" (external directory to mount) and "inode_target" (where to
 * mount on the rootfs)
 */
static void fs_mount_directory(struct inode *inode_src,
                               struct inode *inode_target)
{
    /* nothing to mount */
    if (inode_src->i_size == 0)
        return;

    const uint32_t sb_size = sizeof(struct super_block);
    const uint32_t inode_size = sizeof(struct inode);
    const uint32_t dentry_size = sizeof(struct dentry);

    off_t inode_addr;
    struct inode inode;

    off_t dentry_addr;
    struct dentry dentry;

    /* load the driver file of the storage device */
    struct file *dev_file = mount_points[inode_src->i_rdev].dev_file;
    ssize_t (*dev_read)(struct file * filp, char *buf, size_t size,
                        off_t offset) = dev_file->f_op->read;

    dentry_addr = (uint32_t) inode_src->i_data;

    /*for a mounted directory, reset the block and file size fields of the
     * directory inode since they are copied from the storage device, but the
     * data is not synchronized yet! therefore, they are reset first and will be
     * resumed later after all the files under the directory are mounted.
     */
    if (inode_target->i_sync == false) {
        inode_target->i_size = 0;
        inode_target->i_blocks = 0;
    }

    while (1) {
        /* load the dentry from the storage device */
        dev_read(dev_file, (char *) &dentry, dentry_size, dentry_addr);

        /* load the file inode from the storage device */
        inode_addr = sb_size + (inode_size * dentry.d_inode);
        dev_read(dev_file, (char *) &inode, inode_size, inode_addr);

        /* overwrite the device number */
        inode.i_rdev = inode_src->i_rdev;

        /* mount the file */
        fs_mount_file(inode_target, &inode, &dentry);

        /* calculate the address of the next dentry to read */
        dentry_addr =
            (off_t) list_entry(dentry.d_list.next, struct dentry, d_list);

        /* if the address points to the inodes region, then the iteration is
         * back to the list head */
        if ((uint32_t) dentry.d_list.next <
            (sb_size + inode_size * INODE_CNT_MAX))
            break;  // no more dentry to read
    }

    /* the directory is now synchronized */
    inode_target->i_sync = true;
}

/* get the first entry of a given path. e.g., given a input "dir1/file1.txt"
 * yields with "dir". input : path output: entry name and the reduced string of
 * path
 */
static char *fs_split_path(char *entry, char *path)
{
    while (1) {
        bool found_dir = (*path == '/');

        /* copy */
        if (found_dir == false) {
            *entry = *path;
            entry++;
        }

        path++;

        if ((found_dir == true) || (*path == '\0'))
            break;
    }

    *entry = '\0';

    if (*path == '\0')
        return NULL;  // the path can not be splitted anymore

    return path;  // return the address of the left path string
}

/* create a file by given the pathname
 * input : path name and file type
 * output: file descriptor number
 */
static int fs_create_file(char *pathname, uint8_t file_type)
{
    /* the path name must start with '/' */
    if (pathname[0] != '/')
        return -1;

    struct inode *inode_curr =
        &inodes[0];  // iteration starts from the root node
    struct inode *inode;

    char file_name[FILE_NAME_LEN_MAX];
    char entry[PATH_LEN_MAX];
    char *path = pathname;

    path = fs_split_path(entry, path);  // get rid of the first '/'

    while (1) {
        /* split the path and get the entry name at each layer */
        path = fs_split_path(entry, path);

        /* two successive '/' are detected */
        if (entry[0] == '\0') {
            if (path == NULL) {
                return -1;
            } else {
                continue;
            }
        }

        /* the last non-empty entry string is the file name */
        if (entry[0] != '\0')
            strncpy(file_name, entry, FILE_NAME_LEN_MAX);

        /* search the entry and get the inode */
        inode = fs_search_file(inode_curr, entry);

        if (path != NULL) {
            /* the path can be further splitted, which means it is a directory
             */

            /* check if the directory exists */
            if (inode == NULL) {
                /* directory does not exist, create one */
                inode = fs_add_file(inode_curr, entry, S_IFDIR);

                /* failed to create the directory */
                if (inode == NULL)
                    return -1;
            }

            inode_curr = inode;
        } else {
            /* no more path to be splitted, the remained string should be the
             * file name */

            /* file with the same name already exists */
            if (inode != NULL)
                return -1;

            /* create new inode for the file */
            inode = fs_add_file(inode_curr, file_name, file_type);

            /* failed to create the file */
            if (inode == NULL)
                return -1;

            /* file is created successfully */
            return inode->i_fd;
        }
    }
}

/* open a file by given a pathname
 * input : path name
 * output: file descriptor number
 */
static int fs_open_file(char *pathname)
{
    /* the path name must start with '/' */
    if (pathname[0] != '/')
        return -1;

    struct inode *inode_curr =
        &inodes[0];  // iteration starts from the root node
    struct inode *inode;

    char entry[PATH_LEN_MAX] = {0};
    char *path = pathname;

    path = fs_split_path(entry, path);  // get rid of the first '/'

    while (1) {
        /* split the path and get the entry name at each layer */
        path = fs_split_path(entry, path);

        /* two successive '/' are detected */
        if (entry[0] == '\0') {
            if (path == NULL) {
                return -1;
            } else {
                continue;
            }
        }

        /* search the entry and get the inode */
        inode = fs_search_file(inode_curr, entry);

        /* file or directory not found */
        if (inode == NULL)
            return -1;

        if (path != NULL) {
            /* the path can be further splitted, the iteration can get deeper */

            /* failed, not a directory */
            if (inode->i_mode != S_IFDIR)
                return -1;

            inode_curr = inode;
        } else {
            /* no more path to be splitted, the remained string should be the
             * file name */

            /* make sure the last char is not equal to '/' */
            int len = strlen(pathname);
            if (pathname[len - 1] == '/')
                return -1;

            /* check if the file requires synchronization */
            if ((inode->i_rdev != RDEV_ROOTFS) && (inode->i_sync == false))
                fs_sync_file(inode);  // synchronize the file

            /* failed, not a file */
            if (inode->i_mode == S_IFDIR)
                return -1;

            /* file is opened successfully */
            return inode->i_fd;
        }
    }
}

/* input : file path
 * output: directory inode
 */
struct inode *fs_open_directory(char *pathname)
{
    /* the path name must start with '/' */
    if (pathname[0] != '/')
        return NULL;

    struct inode *inode_curr =
        &inodes[0];  // iteration starts from the root node
    struct inode *inode;

    char entry_curr[PATH_LEN_MAX] = {0};
    char *path = pathname;

    path = fs_split_path(entry_curr, path);  // get rid of the first '/'

    if (path == NULL)
        return inode_curr;

    while (1) {
        /* split the path and get the entry hierarchically */
        path = fs_split_path(entry_curr, path);

        /* two successive '/' are detected */
        if (entry_curr[0] == '\0') {
            if (path == NULL) {
                break;
            } else {
                continue;
            }
        }

        /* search the entry and get the inode */
        inode = fs_search_file(inode_curr, entry_curr);

        /* directory does not exist */
        if (inode == NULL)
            return NULL;

        /* not a directory */
        if (inode->i_mode != S_IFDIR)
            return NULL;

        inode_curr = inode;

        /* no more sub-string to be splitted */
        if (path == NULL)
            break;
    }

    /* synchronize the directory */
    if (inode_curr->i_sync == false)
        fs_mount_directory(inode_curr, inode_curr);

    return inode_curr;
}

static int fs_mount(char *source, char *target)
{
    /* get the file of the storage to be mounted */
    int source_fd = fs_open_file(source);
    if (source_fd < 0)
        return -1;

    /* get the rootfs inode to mount the storage */
    struct inode *target_inode = fs_open_directory(target);
    if (target_inode == NULL)
        return -1;

    /* get storage device file */
    struct file *dev_file = files[source_fd];
    mount_points[mount_cnt].dev_file = dev_file;

    /* get the function pointer of the device read() */
    ssize_t (*dev_read)(struct file * filp, char *buf, size_t size,
                        off_t offset) = dev_file->f_op->read;

    /* calculate the start address of the super block, inode table, and block
     * region */
    const uint32_t sb_size = sizeof(struct super_block);
    const uint32_t inode_size = sizeof(struct inode);
    off_t super_blk_addr = 0;
    off_t inodes_addr = super_blk_addr + sb_size;

    /* read the super block from the device */
    dev_read(dev_file, (char *) &mount_points[mount_cnt].super_blk, sb_size,
             super_blk_addr);

    /* read the root inode of the storage */
    struct inode inode_root;
    dev_read(dev_file, (char *) &inode_root, inode_size, inodes_addr);

    /* overwrite the device number */
    inode_root.i_rdev = mount_cnt;

    /* mount the root directory */
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

        /* switch to the parent directory */
        uint32_t inode_prev = inode->i_ino;
        inode = &inodes[inode->i_parent];

        /* no file is under this directory */
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
        /* handle cd .. */
        fs_getcwd(path_tmp, PATH_LEN_MAX);
        int pos = strlen(path_tmp);
        snprintf(&path_tmp[pos], PATH_LEN_MAX, "..");
    } else {
        /* handle regular path */
        if (path[0] == '/') {
            /* handle the absolute path */
            strncpy(path_tmp, path, PATH_LEN_MAX);
        } else {
            /* handle the relative path */
            fs_getcwd(path_tmp, PATH_LEN_MAX);
            int pos = strlen(path_tmp);
            snprintf(&path_tmp[pos], PATH_LEN_MAX, "%s", path);
        }
    }

    /* open the directory */
    struct inode *inode_dir = fs_open_directory(path_tmp);

    /* directory not found */
    if (!inode_dir) {
        return -ENOENT;
    }

    /* not a directory */
    if (inode_dir->i_mode != S_IFDIR) {
        return -ENOTDIR;
    }

    shell_dir_curr = inode_dir;

    return 0;
}

int fs_read_dir(DIR *dirp, struct dirent *dirent)
{
    /* no dentry to read */
    if (dirp->dentry_list == &dirp->inode_dir->i_dentry)
        return -1;

    /* read the dentry */
    struct dentry *dentry =
        list_entry(dirp->dentry_list, struct dentry, d_list);

    /* copy dirent data */
    dirent->d_ino = dentry->d_inode;
    dirent->d_type = inodes[dentry->d_inode].i_mode;
    strncpy(dirent->d_name, dentry->d_name, FILE_NAME_LEN_MAX);

    /* update the dentry pointer */
    dirp->dentry_list = dirp->dentry_list->next;

    return 0;
}

/* given a block index and return the address of that block */
uint32_t fs_get_block_addr(struct inode *inode, int blk_index)
{
    /* load the device file */
    struct file *dev_file = mount_points[inode->i_rdev].dev_file;

    uint32_t blk_addr = (uint32_t) NULL;
    struct block_header blk_head = {
        .b_next = inode->i_data,
    };  // first block address = inode->i_data

    int i;
    for (i = 0; i < (blk_index + 1); i++) {
        /* get the address of the next block */
        blk_addr = blk_head.b_next;

        /* read the block header */
        dev_file->f_op->read(NULL, (char *) &blk_head,
                             sizeof(struct block_header), blk_addr);
    }

    return blk_addr;
}

void request_create_file(int reply_fd, char *path, uint8_t file_type)
{
    int fs_cmd = FS_CREATE_FILE;
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

    const int filesysd_id = get_daemon_id(FILESYSD);
    fifo_write(files[filesysd_id], buf, buf_size, 0);
}

void request_open_file(int reply_fd, char *path)
{
    int fs_cmd = FS_OPEN_FILE;
    const size_t overhead = sizeof(fs_cmd) + sizeof(reply_fd) + sizeof(path);
    char buf[overhead];
    int buf_size = 0;

    memcpy(&buf[buf_size], &fs_cmd, sizeof(fs_cmd));
    buf_size += sizeof(fs_cmd);

    memcpy(&buf[buf_size], &reply_fd, sizeof(reply_fd));
    buf_size += sizeof(reply_fd);

    memcpy(&buf[buf_size], &path, sizeof(path));
    buf_size += sizeof(path);

    const int filesysd_id = get_daemon_id(FILESYSD);
    fifo_write(files[filesysd_id], buf, buf_size, 0);
}

void request_open_directory(int reply_fd, char *path)
{
    int fs_cmd = FS_OPEN_DIR;
    const size_t overhead = sizeof(fs_cmd) + sizeof(reply_fd) + sizeof(path);
    char buf[overhead];
    int buf_size = 0;

    memcpy(&buf[buf_size], &fs_cmd, sizeof(fs_cmd));
    buf_size += sizeof(fs_cmd);

    memcpy(&buf[buf_size], &reply_fd, sizeof(reply_fd));
    buf_size += sizeof(reply_fd);

    memcpy(&buf[buf_size], &path, sizeof(path));
    buf_size += sizeof(path);

    const int filesysd_id = get_daemon_id(FILESYSD);
    fifo_write(files[filesysd_id], buf, buf_size, 0);
}

void request_mount(int reply_fd, char *source, char *target)
{
    int fs_cmd = FS_MOUNT;
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

    const int filesysd_id = get_daemon_id(FILESYSD);
    fifo_write(files[filesysd_id], buf, buf_size, 0);
}

void request_getcwd(int reply_fd, char *path, size_t len)
{
    int fs_cmd = FS_GET_CWD;
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

    const int filesysd_id = get_daemon_id(FILESYSD);
    fifo_write(files[filesysd_id], buf, buf_size, 0);
}

void request_chdir(int reply_fd, const char *path)
{
    int fs_cmd = FS_CHANGE_DIR;
    const size_t overhead = sizeof(path) + sizeof(reply_fd);
    char buf[overhead];
    int buf_size = 0;

    memcpy(&buf[buf_size], &fs_cmd, sizeof(fs_cmd));
    buf_size += sizeof(fs_cmd);

    memcpy(&buf[buf_size], &reply_fd, sizeof(reply_fd));
    buf_size += sizeof(reply_fd);

    memcpy(&buf[buf_size], &path, sizeof(path));
    buf_size += sizeof(path);

    const int filesysd_id = get_daemon_id(FILESYSD);
    fifo_write(files[filesysd_id], buf, buf_size, 0);
}

void filesysd(void)
{
    setprogname("filesysd");
    set_daemon_id(FILESYSD);

    const int filesysd_id = get_daemon_id(FILESYSD);

    while (1) {
        int file_cmd;
        read(filesysd_id, &file_cmd, sizeof(file_cmd));

        int reply_fd;
        read(filesysd_id, &reply_fd, sizeof(reply_fd));

        switch (file_cmd) {
        case FS_CREATE_FILE: {
            char *path;
            read(filesysd_id, &path, sizeof(path));

            uint8_t file_type;
            read(filesysd_id, &file_type, sizeof(file_type));

            int new_fd = fs_create_file(path, file_type);
            write(reply_fd, &new_fd, sizeof(new_fd));

            break;
        }
        case FS_OPEN_FILE: {
            char *path;
            read(filesysd_id, &path, sizeof(path));

            int open_fd = fs_open_file(path);
            write(reply_fd, &open_fd, sizeof(open_fd));

            break;
        }
        case FS_OPEN_DIR: {
            char *path;
            read(filesysd_id, &path, sizeof(path));

            struct inode *inode_dir = fs_open_directory(path);

            write(reply_fd, &inode_dir, sizeof(inode_dir));

            break;
        }
        case FS_MOUNT: {
            char *source;
            read(filesysd_id, &source, sizeof(source));

            char *target;
            read(filesysd_id, &target, sizeof(target));

            int result = fs_mount(source, target);
            write(reply_fd, &result, sizeof(result));

            break;
        }
        case FS_GET_CWD: {
            char *buf;
            read(filesysd_id, &buf, sizeof(buf));

            size_t len;
            read(filesysd_id, &len, sizeof(len));

            char *retval = fs_getcwd(buf, len);
            write(reply_fd, &retval, sizeof(retval));

            break;
        }
        case FS_CHANGE_DIR: {
            char *path;
            read(filesysd_id, &path, sizeof(path));

            int result = fs_chdir(path);
            write(reply_fd, &result, sizeof(result));

            break;
        }
        }
    }
}

/**
 * debug only:
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
