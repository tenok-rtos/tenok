#include <stdio.h>
#include <string.h>
#include <errno.h>

#include <kernel/kernel.h>
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
