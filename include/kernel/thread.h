/**
 * @file
 */
#ifndef __KERNEL_THREAD_H__
#define __KERNEL_THREAD_H__

#include <stddef.h>

#include <kernel/wait.h>
#include <sys/sched.h>

struct thread_attr {
    struct sched_param schedparam;
    void *stackaddr;
    size_t stacksize; /* bytes */
    int schedpolicy;
    int detachstate;
};

struct thread_once {
    wait_queue_head_t wq;
    bool finished;
};

#endif
