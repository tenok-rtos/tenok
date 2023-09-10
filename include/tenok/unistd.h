#ifndef __UNISTD_H__
#define __UNISTD_H__

#include <stddef.h>
#include <stdint.h>
#include <sys/types.h>

int fork(void);
unsigned int sleep(unsigned int seconds);
int usleep(useconds_t usec);
int close(int fd);
ssize_t read(int fd, void *buf, size_t count);
ssize_t write(int fd, const void *buf, size_t count);
long lseek(int fd, long offset, int whence);
int getpid(void);

#endif
