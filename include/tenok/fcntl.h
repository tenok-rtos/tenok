#ifndef  __FCNTL_H__
#define  __FCNTL_H__

#include <stdio.h>

#ifndef O_NONBLOCK
#define O_NONBLOCK  00004000
#endif

int open(const char *pathname, int flags, mode_t mode);

#endif
