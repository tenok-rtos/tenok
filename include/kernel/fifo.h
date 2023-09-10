#ifndef __FIFO_H__
#define __FIFO_H__

#include <stdio.h>
#include "fs.h"

int fifo_init(int fd, struct file **files, struct inode *file_inode);
ssize_t fifo_read(struct file *filp, char *buf, size_t size, off_t offset);
ssize_t fifo_write(struct file *filp, const char *buf, size_t size, off_t offset);

int mkfifo(const char *pathname, mode_t mode);

#endif
