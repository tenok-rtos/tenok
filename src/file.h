#ifndef __FILE_H__
#define __FILE_H__

struct file {
	const struct file_operations *f_op;
};

struct file_operations {
	long    (*llseek)(struct file *filp, long offset, int whence);
	ssize_t (*read)(struct file *filp, char *buf, size_t size, long offset);
	ssize_t (*write)(struct file *filp, const char *buf, size_t size, long offset);
};

#endif
