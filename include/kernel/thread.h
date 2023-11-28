/**
 * @file
 */
#ifndef __KERNEL_THREAD_H__
#define __KERNEL_THREAD_H__

#include <stddef.h>
#include <sys/sched.h>

#include <common/list.h>

#define CURRENT_THREAD_INFO(var) struct thread_info *var = current_thread_info()

struct thread_attr {
    struct sched_param schedparam;
    void *stackaddr;
    size_t stacksize; /* Bytes */
    int schedpolicy;
    int detachstate;
};

struct thread_once {
    struct list_head wait_list;
    bool finished;
};

struct thread_info *current_thread_info(void);
struct thread_info *acquire_thread(int tid);

#endif
