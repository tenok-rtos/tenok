#ifndef __FILE_H__
#define __FILE_H__

#include <stddef.h>
#include "util.h"

#define S_IFIFO 0
#define S_IFCHR 1
#define S_IFBLK 2
#define S_IFREG 3

struct file {
	const struct file_operations *f_op;
};

struct file_operations {
	long    (*llseek)(struct file *filp, long offset, int whence);
	ssize_t (*read)(struct file *filp, char *buf, size_t size, loff_t offset);
	ssize_t (*write)(struct file *filp, const char *buf, size_t size, loff_t offset);
};

#endif
