#ifndef __FS_H__
#define __FS_H__

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include "list.h"
#include "kconfig.h"

#define S_IFIFO 0 //fifo
#define S_IFCHR 1 //char device
#define S_IFBLK 2 //block device
#define S_IFREG 3 //regular file
#define S_IFDIR 4 //directory

#define RDEV_ROOTFS 0

typedef int  ssize_t;
typedef long loff_t;

enum {
	FS_CREATE_FILE = 1,
	FS_OPEN_FILE   = 2,
	FS_MOUNT       = 3,
} FS_SERVER_CMDS;

struct super_block {
	uint32_t s_blk_cnt;   //number of the used blocks
	uint32_t s_inode_cnt; //number of the used inodes

	uint32_t s_sb_addr;   //start address of the super block
	uint32_t s_ino_addr;  //start address of the inode table
	uint32_t s_blk_addr;  //start address of the blocks region
};

struct mount {
	struct file *dev_file;        //driver file of the mounted storage device
	struct super_block super_blk; //super block of the mounted storage device
};

/* index node */
struct inode {
	uint8_t  i_mode;      //file type: e.g., S_IFIFO, S_IFCHR, etc.

	uint8_t  i_rdev;      //the device on which this file system is mounted
	bool     i_sync;      //the file of a mounted device is loaded into the rootfs or not

	uint32_t i_ino;       //inode number
	uint32_t i_parent;    //inode number of the parent directory

	uint32_t i_fd;        //file descriptor number

	uint32_t i_size;      //file size (bytes)
	uint32_t i_blocks;    //block_numbers = file_size / block_size
	uint8_t  *i_data;     //virtual address for the storage device

	struct list i_dentry; //list head of the dentry table
};

/* directory entry */
struct dentry {
	char     d_name[FILE_NAME_LEN_MAX]; //file name

	uint32_t d_inode;  //the inode of the file
	uint32_t d_parent; //the inode of the parent directory

	struct list d_list;
};

struct stat {
	uint8_t  st_mode;   //file type
	uint32_t st_ino;    //inode number
	uint32_t st_rdev;   //device number
	uint32_t st_size;   //total size in bytes
	uint32_t st_blocks; //number of the blocks used by the file
};

struct file {
	struct inode *file_inode;
	struct file_operations *f_op;
	struct list task_wait_list;
};

struct file_operations {
	long    (*llseek)(struct file *filp, long offset, int whence);
	ssize_t (*read)(struct file *filp, char *buf, size_t size, loff_t offset);
	ssize_t (*write)(struct file *filp, const char *buf, size_t size, loff_t offset);
};

void rootfs_init(void);

int register_chrdev(char *name, struct file_operations *fops);
int register_blkdev(char *name, struct file_operations *fops);

void fs_mount_directory(struct inode *inode_src, struct inode *inode_target);
void fs_print_directory(char *str, struct inode *inode_dir);

void request_create_file(int reply_fd, char *path, uint8_t file_type);
void request_open_file(int reply_fd, char *path);
void request_mount(int reply_fd, char *source, char *path);

void file_system(void);

#endif

