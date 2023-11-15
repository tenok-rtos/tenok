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

#if FILE_DESC_CNT_MAX > 0xffff
#error "FILE_DESC_CNT_MAX should not exceed 65535"
#endif

#define RDEV_ROOTFS 0

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
    uint32_t s_blk_cnt;    // number of the used blocks
    uint32_t s_inode_cnt;  // number of the used inodes

    bool s_rd_only;  // read only flag

    uint32_t s_sb_addr;   // start address of the super block
    uint32_t s_ino_addr;  // start address of the inode table
    uint32_t s_blk_addr;  // start address of the blocks region
};

/* block header will be placed to the top of every blocks of the regular file */
struct block_header {
    uint32_t b_next;  // virtual address of the next block
};

struct mount {
    struct file *dev_file;         // driver file of the mounted storage device
    struct super_block super_blk;  // super block of the mounted storage device
};

/* index node */
struct inode {
    uint8_t i_mode;  // file type: e.g., S_IFIFO, S_IFCHR, etc.

    uint8_t i_rdev;  // the device on which the file is mounted
    bool i_sync;     // the mounted file is loaded into the rootfs or not

    uint32_t i_ino;     // inode number
    uint32_t i_parent;  // inode number of the parent directory

    uint32_t i_fd;  // file descriptor number

    uint32_t i_size;    // file size (bytes)
    uint32_t i_blocks;  // block_numbers = file_size / (block_size -
                        // block_header_size)
    uint32_t i_data;    // virtual address for accessing the storage

    struct list_head i_dentry;  // list head of the dentry table
};

/* directory entry */
struct dentry {
    char d_name[FILE_NAME_LEN_MAX];  // file name
    uint32_t d_inode;                // the inode of the file
    uint32_t d_parent;               // the inode of the parent directory

    struct list_head d_list;
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

int register_chrdev(char *name, struct file_operations *fops);
int register_blkdev(char *name, struct file_operations *fops);

int fs_read_dir(DIR *dirp, struct dirent *dirent);
uint32_t fs_get_block_addr(struct inode *inode, int blk_index);
uint32_t fs_file_append_block(struct inode *inode);

void request_create_file(int reply_fd, char *path, uint8_t file_type);
void request_open_file(int reply_fd, char *path);
void request_open_directory(int reply_fd, char *path);
void request_mount(int reply_fd, char *source, char *path);
void request_getcwd(int reply_fd, char *buf, size_t len);
void request_chdir(int reply_fd, const char *path);

void filesysd(void);

void fs_print_inode_bitmap(void);
void fs_print_block_bitmap(void);

#endif
