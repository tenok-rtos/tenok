#ifndef  __FCNTL_H__
#define  __FCNTL_H__

#define O_NONBLOCK 00004000

int open(const char *pathname, int flags);

#endif
