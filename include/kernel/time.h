#ifndef __KERNEL_TIME_H__
#define __KERNEL_TIME_H__

#include <kernel/list.h>

#include <tenok/time.h>

void sys_time_update_handler(void);

struct timer {
    int id;
    int flags;
    struct sigevent sev;
    struct itimerspec setting;
    struct list list;
};

#endif
