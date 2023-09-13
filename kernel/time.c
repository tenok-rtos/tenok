#include <stdio.h>
#include <string.h>
#include <errno.h>

#include <arch/port.h>
#include <kernel/kernel.h>
#include <kernel/syscall.h>

#include <tenok/time.h>

#include "kconfig.h"

#define SYS_TIM_TICK_PERIOD (1.0f / OS_TICK_FREQ)

time_t time_s = 0;
int tick = 0;

void sys_time_update_handler(void)
{
    tick++;

    if(tick == OS_TICK_FREQ) {
        time_s++;
        tick = 0;
    }
}

int clock_gettime(clockid_t clockid, struct timespec *tp)
{
    tp->tv_sec = time_s;
    tp->tv_nsec = 1000000000 * tick / OS_TICK_FREQ;
}

int timer_create(clockid_t clockid, struct sigevent *sevp,
                 timer_t *timerid)
{
    SYSCALL(TIMER_CREATE);
}

int timer_settime(timer_t timerid, int flags,
                  const struct itimerspec *new_value,
                  struct itimerspec *old_value)
{
    SYSCALL(TIMER_SETTIME);
}

int timer_gettime(timer_t timerid, struct itimerspec *curr_value)
{
    SYSCALL(TIMER_GETTIME);
}
