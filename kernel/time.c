#include <stdio.h>
#include <string.h>
#include <errno.h>

#include <kernel/kernel.h>
#include <tenok/time.h>

#include "kconfig.h"

#define SYS_TIM_TICK_PERIOD (1.0f / OS_TICK_FREQ)

uint32_t tick = 0;
uint32_t time_s = 0;

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
    tp->tv_nsec = SYS_TIM_TICK_PERIOD * (float)tick * 1e9;
}

unsigned int sleep(unsigned int seconds)
{
    delay_ticks(seconds * OS_TICK_FREQ);
    return 0;
}

int usleep(useconds_t usec)
{
    /* FIXME: granularity is too large with current design */

    int usec_tick = OS_TICK_FREQ * (usec / 1000000);
    long granularity = 1000000 / OS_TICK_FREQ;

    if((usec >= 1000000) || (usec_tick < granularity)) {
        return -EINVAL;
    } else {
        delay_ticks(usec_tick);
        return 0;
    }
}
