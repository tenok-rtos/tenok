#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include "mutex.h"
#include "cond.h"
#include "semaphore.h"
#include "mqueue.h"
#include "fs.h"

#define _naked __attribute__ ((naked))

#define SYSCALL(num) \
    asm("push {r7}        \n" \
	"mov  r7, " #num "\n" \
	"svc  0           \n" \
	"nop              \n" \
	"pop  {r7}        \n" \
	"bx lr            \n")

_naked void set_irq(uint32_t state)
{
    SYSCALL(1);
}

_naked void set_program_name(char *name)
{
    SYSCALL(2);
}

_naked void sched_yield(void)
{
    SYSCALL(3);
}

_naked int fork(void)
{
    SYSCALL(4);
}

_naked uint32_t sleep(uint32_t ticks)
{
    SYSCALL(5);
}

_naked int mount(const char *source, const char *target)
{
    SYSCALL(6);
}

_naked int open(const char *pathname, int flags, mode_t mode)
{
    SYSCALL(7);
}

_naked int close(int fd)
{
    SYSCALL(8);
}

_naked ssize_t read(int fd, void *buf, size_t count)
{
    SYSCALL(9);
}

_naked ssize_t write(int fd, const void *buf, size_t count)
{
    SYSCALL(10);
}

_naked long lseek(int fd, long offset, int whence)
{
    SYSCALL(11);
}

_naked int fstat(int fd, struct stat *statbuf)
{
    SYSCALL(12);
}

_naked int opendir(const char *name, DIR *dir)
{
    SYSCALL(13);
}

_naked int readdir(DIR *dirp, struct dirent *dirent)
{
    SYSCALL(14);
}

_naked int getpriority(void)
{
    SYSCALL(15);
}

_naked int setpriority(int which, int who, int prio)
{
    SYSCALL(16);
}

_naked int getpid(void)
{
    SYSCALL(17);
}

_naked int mknod(const char *pathname, mode_t mode, dev_t dev)
{
    SYSCALL(18);
}

_naked int mkfifo(const char *pathname, mode_t mode)
{
    SYSCALL(19);
}

_naked mqd_t mq_open(const char *name, int oflag, struct mq_attr *attr)
{
    SYSCALL(20);
}

_naked ssize_t mq_receive(mqd_t mqdes, char *msg_ptr, size_t msg_len, unsigned int *msg_prio)
{
    SYSCALL(21);
}

_naked int mq_send(mqd_t mqdes, const char *msg_ptr, size_t msg_len, unsigned int msg_prio)
{
    SYSCALL(22);
}

_naked int pthread_mutex_init(pthread_mutex_t *mutex, const pthread_mutex_attr_t *attr)
{
    SYSCALL(23);
}

_naked int pthread_mutex_unlock(pthread_mutex_t *mutex)
{
    SYSCALL(24);
}

_naked int pthread_mutex_lock(pthread_mutex_t *mutex)
{
    SYSCALL(25);
}

_naked int pthread_cond_init(pthread_cond_t *cond, pthread_condattr_t cond_attr)
{
    SYSCALL(26);
}

_naked int pthread_cond_signal(pthread_cond_t *cond)
{
    SYSCALL(27);
}

_naked int pthread_cond_wait(pthread_cond_t *cond, pthread_mutex_t *mutex)
{
    SYSCALL(28);
}

_naked int sem_init(sem_t *sem, int pshared, unsigned int value)
{
    SYSCALL(29);
}

_naked int sem_post(sem_t *sem)
{
    SYSCALL(30);
}

_naked int sem_trywait(sem_t *sem)
{
    SYSCALL(31);
}

_naked int sem_wait(sem_t *sem)
{
    SYSCALL(32);
}

_naked int sem_getvalue(sem_t *sem, int *sval)
{
    SYSCALL(33);
}
