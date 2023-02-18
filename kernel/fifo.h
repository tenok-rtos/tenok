#ifndef __FIFO_H__
#define __FIFO_H__

#include <stdio.h>
#include "fs.h"
#include "ringbuf.h"
#include "mpool.h"

int mkfifo(const char *pathname, mode_t mode);

int fifo_init(int fd, struct file **files, struct memory_pool *mem_pool);
ssize_t fifo_read(struct file *filp, char *buf, size_t size, loff_t offset);
ssize_t fifo_write(struct file *filp, const char *buf, size_t size, loff_t offset);

#endif
