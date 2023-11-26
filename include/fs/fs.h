/**
 * @file
 */
#ifndef __FS_H__
#define __FS_H__

#include <dirent.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <sys/types.h>

#include <common/list.h>
#include <kernel/wait.h>

#include "kconfig.h"

#define RDEV_ROOTFS 0

#define FILE_RESERVED_NUM (THREAD_MAX + 3)
#define THREAD_PIPE_FD(thread_id) (thread_id + 3)

/* +---------------------------+
 * |     File table layout     |
 * +-----------+---------------+
 * |     0     |     stdin     |
 * +-----------+---------------+
 * |     1     |     stdout    |
 * +-----------+---------------+
 * |     2     |     stderr    |
 * +-----------+---------------+
 * |     3     | Thread pipe 1 |
 * +-----------+---------------+
 * |    ...    |      ...      |
 * +-----------+---------------+
 * |   N + 2   | Thread pipe N |
 * +-----------+---------------+
 * |   N + 3   |     File 1    |
 * +-----------+---------------+
 * |    ...    |      ...      |
 * +-----------+---------------+
 * | N + M + 3 |     File M    |
 * +-----------+---------------+
 *
 * N = THREAD_MAX
 * M = OPEN_MAX
 */

typedef void (*drv_init_func_t)(void);

enum {
    FS_CREATE_FILE = 1,
    FS_OPEN_FILE = 2,
    FS_OPEN_DIR = 3,
    FS_MOUNT = 4,
    FS_GET_CWD = 5,
    FS_CHANGE_DIR = 6,
} FS_SERVER_CMDS;

struct super_block {
    bool s_rd_only;       /* Read-only flag */
    uint32_t s_blk_cnt;   /* number of the used blocks */
    uint32_t s_inode_cnt; /* number of the used inodes */
    uint64_t s_sb_addr;   /* Start address of the super block */
    uint64_t s_ino_addr;  /* Start address of the inode table */
    uint64_t s_blk_addr;  /* Start address of the blocks region */
};

/* Block header will be placed at the top of every blocks of the regular file */
struct block_header {
    /* Virtual address of the next block */
    uint32_t b_next;
    uint32_t reserved;
};

struct mount {
    struct file *dev_file;        /* Driver file of the mounted device */
    struct super_block super_blk; /* Super block of the mounted device */
};

/* index node */
struct inode {
    /* File type: e.g., S_IFIFO, S_IFCHR, etc. */
    uint8_t i_mode;
    /* The device on which the file is mounted */
    uint8_t i_rdev;
    /* The mounted file is loaded into the rootfs or not */
    bool i_sync;
    /* inode number */
    uint32_t i_ino;
    /* inode number of the parent directory */
    uint32_t i_parent;
    /* File descriptor number */
    uint32_t i_fd;
    /* File size (bytes) */
    uint32_t i_size;
    /* block_numbers = file_size / (block_size - block_header_size) */
    uint32_t i_blocks;
    /* Virtual address for accessing the storage */
    uint32_t i_data;
    uint32_t reserved1;
    /* List head of the dentry table */
    struct list_head i_dentry;
    uint32_t reserved2[2];
};

/* Directory entry */
struct dentry {
    /* File name */
    char d_name[NAME_MAX];
    /* The inode of the file */
    uint32_t d_inode;
    /* The inode of the parent directory */
    uint32_t d_parent;
    /* List head of the dentry */
    struct list_head d_list;
    uint32_t reserved[2];
};

struct file {
    struct inode *f_inode;
    struct file_operations *f_op;
    uint32_t f_events;
    int f_flags;
    struct list_head list;
};

struct file_operations {
    off_t (*lseek)(struct file *filp, off_t offset, int whence);
    ssize_t (*read)(struct file *filp, char *buf, size_t size, off_t offset);
    ssize_t (*write)(struct file *filp,
                     const char *buf,
                     size_t size,
                     off_t offset);
    int (*ioctl)(struct file *, unsigned int cmd, unsigned long arg);
    int (*open)(struct inode *inode, struct file *file);
};

struct fdtable {
    int flags;
    struct file *file;
};

void rootfs_init(void);

void link_stdin_dev(char *path);
void link_stdout_dev(char *path);
void link_stderr_dev(char *path);

int register_chrdev(char *name, struct file_operations *fops);
int register_blkdev(char *name, struct file_operations *fops);

int fs_read_dir(DIR *dirp, struct dirent *dirent);
uint32_t fs_get_block_addr(struct inode *inode, int blk_index);
uint32_t fs_file_append_block(struct inode *inode);

void request_create_file(int thread_id, const char *path, uint8_t file_type);
void request_open_file(int thread_id, const char *path);
void request_open_directory(int reply_fd, const char *path);
void request_mount(int thread_id, const char *source, const char *target);
void request_getcwd(int thread_id, char *buf, size_t len);
void request_chdir(int thread_id, const char *path);

void filesysd(void);

void fs_print_inode_bitmap(void);
void fs_print_block_bitmap(void);

#endif
