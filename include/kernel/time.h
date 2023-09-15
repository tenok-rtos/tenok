#ifndef __KERNEL_TIME_H__
#define __KERNEL_TIME_H__

#include <stdbool.h>

#include <kernel/list.h>

#include <tenok/time.h>

struct time {
    time_t seconds;
    int ticks;
};

struct timer {
    int id;
    int flags;
    bool enabled;

    struct sigevent sev;
    struct itimerspec setting;

    struct timespec next;
    struct timespec now;

    struct list list;
};

void timer_reset(struct timespec *time);
void timer_tick_update(struct timespec *time);
void sys_time_update_handler(void);

#endif
