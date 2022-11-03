#ifndef __SYSCALL_H__
#define __SYSCALL_H__

#include <stddef.h>
#include <stdint.h>
#include "list.h"
#include "util.h"

#define DEF_SYSCALL(func, _num) \
	{.syscall_handler = sys_ ## func, .num = _num}

typedef struct {
	void (* syscall_handler)(void);
	uint32_t num;
} syscall_info_t;

typedef struct {
	uint32_t count;
	list_t wait_list;
} sem_t;

uint32_t sem_up(uint32_t *_lock, uint32_t new_val);
uint32_t sem_down(uint32_t *_lock, uint32_t new_val);

/* syscall function prototypes */
int fork(void);
uint32_t sleep(uint32_t ticks);
ssize_t read(int fd, void *buf, size_t count);
ssize_t write(int fd, const void *buf, size_t count);
uint32_t getpriority(void);
int setpriority(int which, int who, int prio);
int getpid(void);
int mknod(const char *pathname, mode_t mode, dev_t dev);

#endif
