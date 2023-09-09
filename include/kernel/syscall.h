#ifndef __SYSCALL_H__
#define __SYSCALL_H__

#include <stdio.h>
#include <stddef.h>
#include <stdint.h>
#include "list.h"
#include "mqueue.h"
#include "semaphore.h"
#include "cond.h"
#include "fs.h"
#include "mutex.h"

void sched_yield(void);
void set_irq(uint32_t state);
void set_program_name(char *name);
int fork(void);
uint32_t sleep(uint32_t ticks);
int mount(const char *source, const char *target);
int open(const char *pathname, int flags, mode_t mode);
int close(int fd);
ssize_t read(int fd, void *buf, size_t count);
ssize_t write(int fd, const void *buf, size_t count);
long lseek(int fd, long offset, int whence);
int fstat(int fd, struct stat *statbuf);
int opendir(const char *name, DIR *dir);
int readdir(DIR *dirp, struct dirent *dirent);
int getpriority(void);
int setpriority(int which, int who, int prio);
int getpid(void);
int mknod(const char *pathname, mode_t mode, dev_t dev);
int mkfifo(const char *pathname, mode_t mode);
int pthread_cond_init(pthread_cond_t *cond, pthread_condattr_t cond_attr);
int pthread_cond_signal(pthread_cond_t *cond);
int pthread_cond_wait(pthread_cond_t *cond, pthread_mutex_t *mutex);
int pthread_mutex_init(pthread_mutex_t *mutex, const pthread_mutex_attr_t *attr);
int pthread_mutex_unlock(pthread_mutex_t *mutex);
int pthread_mutex_lock(pthread_mutex_t *mutex);
int sem_init(sem_t *sem, int pshared, unsigned int value);
int sem_post(sem_t *sem);
int sem_trywait(sem_t *sem);
int sem_wait(sem_t *sem);
int sem_getvalue(sem_t *sem, int *sval);

#endif
