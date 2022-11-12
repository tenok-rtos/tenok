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
} pthread_mutex_t;

typedef void pthread_mutex_attr_t;

int pthread_mutex_init(pthread_mutex_t *mutex, const pthread_mutex_attr_t *attr);
int pthread_mutex_unlock(pthread_mutex_t *mutex);
int pthread_mutex_lock(pthread_mutex_t *mutex);

#endif
