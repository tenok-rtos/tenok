#include <stdio.h>
#include <string.h>
#include "time.h"
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
}
