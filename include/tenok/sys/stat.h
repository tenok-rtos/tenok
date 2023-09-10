#ifndef __STAT_H__
#define __STAT_H__

#include "fs.h"

int fstat(int fd, struct stat *statbuf);
int mknod(const char *pathname, mode_t mode, dev_t dev);
int mkfifo(const char *pathname, mode_t mode);

#endif
