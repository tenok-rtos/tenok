#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include "syscall.h"
#include "mutex.h"
#include "cond.h"
#include "semaphore.h"
#include "mqueue.h"
#include "fs.h"

#define _naked __attribute__ ((naked))

#define SYSCALL(num) \
    asm volatile ("push {r7}   \n" \
    	          "mov  r7, %0 \n" \
	          "svc  0      \n" \
	          "nop         \n" \
	          "pop  {r7}   \n" \
	          "bx lr       \n" \
                  :: "i"(num))

_naked void set_irq(uint32_t state)
{
    SYSCALL(SET_IRQ);
}

_naked void set_program_name(char *name)
{
    SYSCALL(SET_PROGRAM_NAME);
}

_naked uint32_t delay_ticks(uint32_t ticks)
{
    SYSCALL(DELAY_TICKS);
}

_naked void sched_yield(void)
{
    SYSCALL(SCHED_YIELD);
}

_naked int fork(void)
{
    SYSCALL(FORK);
}

_naked int mount(const char *source, const char *target)
{
    SYSCALL(MOUNT);
}

_naked int open(const char *pathname, int flags, mode_t mode)
{
    SYSCALL(OPEN);
}

_naked int close(int fd)
{
    SYSCALL(CLOSE);
}

_naked ssize_t read(int fd, void *buf, size_t count)
{
    SYSCALL(READ);
}

_naked ssize_t write(int fd, const void *buf, size_t count)
{
    SYSCALL(WRITE);
}

_naked long lseek(int fd, long offset, int whence)
{
    SYSCALL(LSEEK);
}

_naked int fstat(int fd, struct stat *statbuf)
{
    SYSCALL(FSTAT);
}

_naked int opendir(const char *name, DIR *dir)
{
    SYSCALL(OPENDIR);
}

_naked int readdir(DIR *dirp, struct dirent *dirent)
{
    SYSCALL(READDIR);
}

_naked int getpriority(void)
{
    SYSCALL(GETPRIORITY);
}

_naked int setpriority(int which, int who, int prio)
{
    SYSCALL(SETPRIORITY);
}

_naked int getpid(void)
{
    SYSCALL(GETPID);
}

_naked int mknod(const char *pathname, mode_t mode, dev_t dev)
{
    SYSCALL(MKNOD);
}

_naked int mkfifo(const char *pathname, mode_t mode)
{
    SYSCALL(MKFIFO);
}

_naked mqd_t mq_open(const char *name, int oflag, struct mq_attr *attr)
{
    SYSCALL(MQ_OPEN);
}

_naked ssize_t mq_receive(mqd_t mqdes, char *msg_ptr, size_t msg_len, unsigned int *msg_prio)
{
    SYSCALL(MQ_RECEIVE);
}

_naked int mq_send(mqd_t mqdes, const char *msg_ptr, size_t msg_len, unsigned int msg_prio)
{
    SYSCALL(MQ_SEND);
}

_naked int pthread_mutex_init(pthread_mutex_t *mutex, const pthread_mutex_attr_t *attr)
{
    SYSCALL(PTHREAD_MUTEX_INIT);
}

_naked int pthread_mutex_unlock(pthread_mutex_t *mutex)
{
    SYSCALL(PTHREAD_MUTEX_UNLOCK);
}

_naked int pthread_mutex_lock(pthread_mutex_t *mutex)
{
    SYSCALL(PTHREAD_MUTEX_LOCK);
}

_naked int pthread_cond_init(pthread_cond_t *cond, pthread_condattr_t cond_attr)
{
    SYSCALL(PTHREAD_COND_INIT);
}

_naked int pthread_cond_signal(pthread_cond_t *cond)
{
    SYSCALL(PTHREAD_COND_SIGNAL);
}

_naked int pthread_cond_wait(pthread_cond_t *cond, pthread_mutex_t *mutex)
{
    SYSCALL(PTHREAD_COND_WAIT);
}

_naked int sem_init(sem_t *sem, int pshared, unsigned int value)
{
    SYSCALL(SEM_INIT);
}

_naked int sem_post(sem_t *sem)
{
    SYSCALL(SEM_POST);
}

_naked int sem_trywait(sem_t *sem)
{
    SYSCALL(SEM_TRYWAIT);
}

_naked int sem_wait(sem_t *sem)
{
    SYSCALL(SEM_WAIT);
}

_naked int sem_getvalue(sem_t *sem, int *sval)
{
    SYSCALL(SEM_GETVALUE);
}
