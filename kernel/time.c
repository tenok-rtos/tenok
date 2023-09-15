#include <stdio.h>
#include <string.h>
#include <errno.h>

#include <arch/port.h>
#include <kernel/kernel.h>
#include <kernel/syscall.h>

#include <tenok/time.h>

#include "kconfig.h"

#define NANOSECOND_TICKS (1000000000 / OS_TICK_FREQ)

time_t time_s = 0;
long time_ns = 0;

void sys_time_update_handler(void)
{
    time_ns += NANOSECOND_TICKS;

    if(time_ns >= 1000000000) {
        time_s++;
        time_ns -= 1000000000;
    }
}

int clock_gettime(clockid_t clockid, struct timespec *tp)
{
    tp->tv_sec = time_s;
    tp->tv_nsec = time_ns;
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
