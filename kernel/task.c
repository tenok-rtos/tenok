#include <stdint.h>
#include <errno.h>

#include <arch/port.h>
#include <kernel/syscall.h>

#include <tenok/sys/types.h>

#include "kconfig.h"

NACKED void set_program_name(char *name)
{
    SYSCALL(SET_PROGRAM_NAME);
}

NACKED uint32_t delay_ticks(uint32_t ticks)
{
    SYSCALL(DELAY_TICKS);
}

NACKED void sched_yield(void)
{
    SYSCALL(SCHED_YIELD);
}

NACKED int fork(void)
{
    SYSCALL(FORK);
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
