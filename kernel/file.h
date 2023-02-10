#ifndef __FILE_H__
#define __FILE_H__

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include "util.h"
#include "list.h"
#include "kconfig.h"

#define S_IFIFO 0
#define S_IFCHR 1
#define S_IFBLK 2
#define S_IFREG 3

enum {
	FS_CREATE_FILE = 1,
	FS_OPEN_FILE = 2,
} PATH_SERVER_CMDS;

struct inode {
	char     name[FILE_NAME_LEN_MAX];
	bool     is_dir;

	uint8_t  *data;
	int      data_size;
};

struct dir_info {
	char     entry_name[FILE_NAME_LEN_MAX];
	uint32_t entry_inode;

	struct dir_info *next;
};

struct file {
	struct file_operations *f_op;
	struct list task_wait_list;
};

struct file_operations {
	long    (*llseek)(struct file *filp, long offset, int whence);
	ssize_t (*read)(struct file *filp, char *buf, size_t size, loff_t offset);
	ssize_t (*write)(struct file *filp, const char *buf, size_t size, loff_t offset);
};

void request_path_register(int reply_fd, char *path);
void request_file_open(int reply_fd, char *path);

void file_system(void);

#endif

