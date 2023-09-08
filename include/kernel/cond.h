#ifndef __COND_H__
#define __COND_H__

#define pthread_cond_t     __pthread_cond_t
#define pthread_condattr_t uint32_t //no support, ignored

#include "list.h"

typedef struct {
    struct list task_wait_list;
} __pthread_cond_t;

#endif
