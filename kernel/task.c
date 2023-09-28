#include <stdint.h>
#include <errno.h>

#include <arch/port.h>
#include <kernel/kernel.h>
#include <kernel/syscall.h>

#include <tenok/tenok.h>
#include <tenok/sys/types.h>

#include "kconfig.h"

NACKED int procstat(struct procstat_info info[TASK_CNT_MAX])
{
    SYSCALL(PROCSTAT);
}

NACKED void setprogname(const char *name)
{
    SYSCALL(SETPROGNAME);
}

NACKED const char *getprogname(void)
{
    SYSCALL(GETPROGNAME);
}

NACKED uint32_t delay_ticks(uint32_t ticks)
{
    SYSCALL(DELAY_TICKS);
}


inline int sched_get_priority_max(int policy)
{
    return TASK_MAX_PRIORITY;
}

inline int sched_get_priority_min(int policy)
{
    return 0;
}

NACKED void sched_yield(void)
{
    SYSCALL(SCHED_YIELD);
}

NACKED void _exit(int status)
{
    SYSCALL(_EXIT);
}

NACKED int getpriority(void)
{
    SYSCALL(GETPRIORITY);
}

NACKED int setpriority(int which, int who, int prio)
{
    SYSCALL(SETPRIORITY);
}

NACKED int getpid(void)
{
    SYSCALL(GETPID);
}

unsigned int sleep(unsigned int seconds)
{
    delay_ticks(seconds * OS_TICK_FREQ);
    return 0;
}

NACKED int task_create(task_func_t task_func, uint8_t priority, int stack_size)
{
    SYSCALL(TASK_CREATE);
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
