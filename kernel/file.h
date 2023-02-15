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

enum {
	FS_CREATE_FILE = 1,
	FS_OPEN_FILE   = 2,
} FS_SERVER_CMDS;

struct super_block {
	int inode_cnt;
	int blk_cnt;
};

/* index node */
struct inode {
	uint8_t  i_mode;   //file type: e.g., S_IFIFO, S_IFCHR, etc.

	uint32_t i_ino;    //inode number
	uint32_t i_parent; //inode number of the parent directory

	uint32_t i_size;   //file size (bytes)
	uint32_t i_blocks; //i_block = file_size / block_size
	uint8_t  *i_data;
};

/* directory entry */
struct dentry {
	char     file_name[FILE_NAME_LEN_MAX]; //file name

	uint32_t file_inode;   //the inode of this file
	uint32_t parent_inode; //the parent directory's inode of this file

	struct dentry *next;
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

void request_create_file(int reply_fd, char *path, uint8_t file_type);
void request_open_file(int reply_fd, char *path);

void file_system(void);

#endif

