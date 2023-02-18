#ifndef __SYSCALL_H__
#define __SYSCALL_H__

#include <stdio.h>
#include <stddef.h>
#include <stdint.h>
#include "list.h"
#include "mqueue.h"
#include "semaphore.h"
#include "fs.h"

void sched_yield(void);
void set_irq(uint32_t state);
void set_program_name(char *name);
int fork(void);
uint32_t sleep(uint32_t ticks);
int mount(const char *source, const char *target);
int open(const char *pathname, int flags, mode_t);
int close(int fd);
ssize_t read(int fd, void *buf, size_t count);
ssize_t write(int fd, const void *buf, size_t count);
long lseek(int fd, long offset, int whence);
int fstat(int fd, struct stat *statbuf);
uint32_t getpriority(void);
int setpriority(int which, int who, int prio);
int getpid(void);
int mknod(const char *pathname, mode_t mode, dev_t dev);
mqd_t mq_open(const char *name, int oflag, struct mq_attr *attr);
int mq_send(mqd_t mqdes, const char *msg_ptr, size_t msg_len, unsigned int msg_prio);
ssize_t mq_receive(mqd_t mqdes, char *msg_ptr, size_t msg_len, unsigned int *msg_prio);
int mq_close(mqd_t mqdes);
int os_sem_wait(sem_t *sem);

#endif
