#ifndef __SYSCALL_H__
#define __SYSCALL_H__

#include <stddef.h>
#include <stdint.h>
#include "list.h"
#include "util.h"
#include "mqueue.h"

#define DEF_SYSCALL(func, _num) \
	{.syscall_handler = sys_ ## func, .num = _num}

typedef struct {
	void (* syscall_handler)(void);
	uint32_t num;
} syscall_info_t;

/* syscall function prototypes */
void yield(void);
void set_irq(uint32_t state);
int fork(void);
uint32_t sleep(uint32_t ticks);
int open(const char *pathname, int flags, _mode_t);
int close(int fd);
ssize_t read(int fd, void *buf, size_t count);
ssize_t write(int fd, const void *buf, size_t count);
uint32_t getpriority(void);
int setpriority(int which, int who, int prio);
int getpid(void);
int mknod(const char *pathname, _mode_t mode, _dev_t dev);
int mkdir(const char *pathname, _mode_t mode);
int rmdir(const char *pathname);

#endif
