#include <stdio.h>
#include <stddef.h>
#include <string.h>
#include <errno.h>
#include "file.h"
#include "syscall.h"

long reg_file_llseek(struct file *filp, long offset, int whence);
ssize_t reg_file_read(struct file *filp, char *buf, size_t size, loff_t offset);
ssize_t reg_file_write(struct file *filp, const char *buf, size_t size, loff_t offset);

static struct file_operations reg_file_ops = {
	.llseek = reg_file_llseek,
	.read = reg_file_read,
	.write = reg_file_write
};

ssize_t reg_file_read(struct file *filp, char *buf, size_t size, loff_t offset)
{
	return size;
}

ssize_t reg_file_write(struct file *filp, const char *buf, size_t size, loff_t offset)
{
	return 0;
}

long reg_file_llseek(struct file *filp, long offset, int whence)
{
	switch(whence) {
	case SEEK_SET:
		break;
	case SEEK_END:
		break;
	case SEEK_CUR:
		break;
	default:
		return -1;
	}
}
