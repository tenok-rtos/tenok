#ifndef __MUTEX_H__
#define __MUTEX_H__

#include "kernel.h"
#include "spinlock.h"
#include "list.h"

#define EPERM 1  //the current thread does not own the mutex

typedef struct {
    spinlock_t lock;
    tcb_t      *owner;
    list_t     wait_list;
} _pthread_mutex_t;

typedef void pthread_mutex_attr_t;

int pthread_mutex_init(_pthread_mutex_t *mutex, const pthread_mutex_attr_t *attr);
int pthread_mutex_unlock(_pthread_mutex_t *mutex);
int pthread_mutex_lock(_pthread_mutex_t *mutex);

#endif
