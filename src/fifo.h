#ifndef __FIFO_H__
#define __FIFO_H__

#include "file.h"
#include "ringbuf.h"

int mkfifo(const char *pathname, mode_t mode);

#endif
