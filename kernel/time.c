#include <stdio.h>
#include <string.h>
#include <errno.h>

#include <arch/port.h>
#include <kernel/kernel.h>
#include <kernel/syscall.h>

#include <tenok/time.h>

#include "kconfig.h"

#define NANOSECOND_TICKS (1000000000 / OS_TICK_FREQ)

struct timespec sys_time;

void timer_reset(struct timespec *time)
{
    time->tv_sec = 0;
    time->tv_nsec = 0;
}

void timer_tick_update(struct timespec *time)
{
    time->tv_nsec += NANOSECOND_TICKS;

    if(time->tv_nsec >= 1000000000) {
        time->tv_sec++;
        time->tv_nsec -= 1000000000;
    }
}

void sys_time_update_handler(void)
{
    timer_tick_update(&sys_time);
}

int clock_gettime(clockid_t clockid, struct timespec *tp)
{
    *tp = sys_time;
}

NACKED int timer_create(clockid_t clockid, struct sigevent *sevp,
                        timer_t *timerid)
{
    SYSCALL(TIMER_CREATE);
}

NACKED int timer_delete(timer_t timerid)
{
    SYSCALL(TIMER_DELETE);
}

NACKED int timer_settime(timer_t timerid, int flags,
                         const struct itimerspec *new_value,
                         struct itimerspec *old_value)
{
    SYSCALL(TIMER_SETTIME);
}

NACKED int timer_gettime(timer_t timerid, struct itimerspec *curr_value)
{
    SYSCALL(TIMER_GETTIME);
}
