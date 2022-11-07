#ifndef __MQUEUE_H__
#define __MQUEUE_H__

#include "file.h"
#include "ringbuf.h"
#include "mpool.h"

int mq_init(int fd, struct file **files, struct memory_pool *mem_pool);

#endif
