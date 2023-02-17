#ifndef __FILE_H__
#define __FILE_H__

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include "util.h"
#include "list.h"
#include "kconfig.h"

#define S_IFIFO 0 //fifo
#define S_IFCHR 1 //char device
#define S_IFBLK 2 //block device
#define S_IFREG 3 //regular file
#define S_IFDIR 4 //directory

#define RDEV_ROOTFS 0

enum {
	FS_CREATE_FILE = 1,
	FS_OPEN_FILE   = 2,
} FS_SERVER_CMDS;

struct super_block {
	uint32_t blk_cnt;
	uint32_t inode_cnt;
};

struct mount {
	struct file *dev_file;
	struct super_block super_blk;
};

/* index node */
struct inode {
	uint8_t  i_mode;      //file type: e.g., S_IFIFO, S_IFCHR, etc.

	uint8_t  i_rdev;      //the device on which this file system is mounted

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
	char     file_name[FILE_NAME_LEN_MAX]; //file name

	uint32_t file_inode;   //the inode of the file
	uint32_t parent_inode; //the parent directory's inode of the file

	struct list list;
};

struct stat {
	uint8_t st_mode;
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

void file_system_init(void);

int register_chrdev(char *name, struct file_operations *fops);
int register_blkdev(char *name, struct file_operations *fops);

int fs_write(struct inode *inode, uint8_t *write_addr, uint8_t *data, size_t size);
int fs_read(struct inode *inode, uint8_t *read_addr, uint8_t *data, size_t size);

void request_create_file(int reply_fd, char *path, uint8_t file_type);
void request_open_file(int reply_fd, char *path);

void file_system(void);

#endif

