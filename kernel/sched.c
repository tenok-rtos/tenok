#include <tenok.h>
#include <time.h>
#include <errno.h>

#include <arch/port.h>
#include <kernel/kernel.h>
#include <kernel/syscall.h>

#include "kconfig.h"

inline int sched_get_priority_max(int policy)
{
    return TASK_MAX_PRIORITY;
}

inline int sched_get_priority_min(int policy)
{
    return 0;
}

int sched_rr_get_interval(pid_t pid, struct timespec *tp)
{
    tp->tv_sec = OS_TICK_FREQ / 1000;
    tp->tv_nsec = (1000000000 / OS_TICK_FREQ) % 1000000000;

    return 0;
}

NACKED void sched_yield(void)
{
    SYSCALL(SCHED_YIELD);
}

NACKED int delay_ticks(uint32_t ticks)
{
    SYSCALL(DELAY_TICKS);
}

unsigned int sleep(unsigned int seconds)
{
    delay_ticks(seconds * OS_TICK_FREQ);
    return 0;
}

int usleep(useconds_t usec)
{
    /* FIXME: granularity is too large with current design */

    int usec_tick = OS_TICK_FREQ * usec / 1000000;
    long granularity_us = 1000000 / OS_TICK_FREQ;

    if((usec >= 1000000) || (usec < granularity_us)) {
        return -EINVAL;
    } else {
        delay_ticks(usec_tick);
        return 0;
    }
}
