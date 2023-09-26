#ifndef __PTHREAD_H__
#define __PTHREAD_H__

#include <stdint.h>

#include <kernel/list.h>
#include <kernel/spinlock.h>

typedef void pthread_mutex_attr_t;   /* no usage */
typedef uint32_t pthread_condattr_t; /* no usage */
typedef uint32_t pthread_once_t;

typedef struct {
    spinlock_t lock;
    struct task_ctrl_blk *owner;
    struct list wait_list;
} pthread_mutex_t;

typedef struct {
    struct list task_wait_list;
} pthread_cond_t;

int pthread_mutex_init(pthread_mutex_t *mutex, const pthread_mutex_attr_t *attr);
int pthread_mutex_unlock(pthread_mutex_t *mutex);
int pthread_mutex_lock(pthread_mutex_t *mutex);

int pthread_cond_init(pthread_cond_t *cond, pthread_condattr_t cond_attr);
int pthread_cond_signal(pthread_cond_t *cond);
int pthread_cond_wait(pthread_cond_t *cond, pthread_mutex_t *mutex);

int pthread_once(pthread_once_t *once_control, void (*init_routine)(void));

#endif
