#ifndef __KERNEL_TIME_H__
#define __KERNEL_TIME_H__

#include <stdbool.h>

#include <kernel/list.h>

#include <tenok/time.h>

struct timer {
    int  id;
    int  flags;
    bool enabled;
    struct sigevent sev;
    struct itimerspec setting;
    struct itimerspec ret_time; /* for returning to the getter */
    struct timespec counter;    /* internal down counter */
    struct list list;
};

void timer_up_count(struct timespec *time);
void timer_down_count(struct timespec *time);
void sys_time_update_handler(void);

#endif
