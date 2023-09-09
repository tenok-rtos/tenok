#ifndef __MUTEX_H__
#define __MUTEX_H__

#include "kernel.h"
#include "spinlock.h"
#include "list.h"

#define pthread_mutex_t __pthread_mutex_t

#define EPERM 1  //the current thread does not own the mutex

typedef struct {
    spinlock_t lock;
    struct task_ctrl_blk *owner;
    struct list wait_list;
} __pthread_mutex_t;

typedef void pthread_mutex_attr_t;

#endif
