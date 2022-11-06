#ifndef __FIFO_H__
#define __FIFO_H__

#include "file.h"
#include "ringbuf.h"
#include "mpool.h"

int mkfifo(const char *pathname, _mode_t mode);

int fifo_init(int fd, struct file **files, struct memory_pool *mem_pool);

#endif
