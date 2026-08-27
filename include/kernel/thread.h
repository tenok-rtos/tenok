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
    int inheritsched; /* Whether the two scheduling fields are used at all */
    int detachstate;
};

struct thread_once {
    struct list_head wait_list;
    bool running;  /* A thread is running the routine right now */
    bool finished; /* The routine has been run and will not be run again */
};

struct thread_info *current_thread_info(void);
struct thread_info *acquire_thread(int tid);

/* Run in the thread that is ending, from either of the two ways out of one */
void __run_tls_destructors(void);

#endif
