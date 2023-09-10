#ifndef __PTHREAD_H__
#define __PTHREAD_H__

#include "list.h"
#include "spinlock.h"

#define pthread_mutex_t    __pthread_mutex_t
#define pthread_cond_t     __pthread_cond_t
#define pthread_condattr_t uint32_t

typedef void pthread_mutex_attr_t;

typedef struct {
    spinlock_t lock;
    struct task_ctrl_blk *owner;
    struct list wait_list;
} __pthread_mutex_t;

typedef struct {
    struct list task_wait_list;
} __pthread_cond_t;

int pthread_mutex_init(pthread_mutex_t *mutex, const pthread_mutex_attr_t *attr);
int pthread_mutex_unlock(pthread_mutex_t *mutex);
int pthread_mutex_lock(pthread_mutex_t *mutex);

int pthread_cond_init(pthread_cond_t *cond, pthread_condattr_t cond_attr);
int pthread_cond_signal(pthread_cond_t *cond);
int pthread_cond_wait(pthread_cond_t *cond, pthread_mutex_t *mutex);

#endif
