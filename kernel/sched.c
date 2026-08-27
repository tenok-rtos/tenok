#include <errno.h>
#include <stdint.h>
#include <sys/types.h>
#include <tenok.h>
#include <time.h>

#include <arch/port.h>
#include <kernel/syscall.h>

#include "kconfig.h"

int sched_get_priority_max(int policy)
{
    return THREAD_PRIORITY_MAX;
}

int sched_get_priority_min(int policy)
{
    return 0;
}

int sched_rr_get_interval(pid_t pid, struct timespec *tp)
{
    if (!tp)
        return -EINVAL;

    uint64_t ns = 1000000000ULL / OS_TICK_FREQ;
    tp->tv_sec = ns / 1000000000ULL;
    tp->tv_nsec = ns % 1000000000ULL;

    return 0;
}

NACKED int sched_yield(void)
{
    SYSCALL(SCHED_YIELD);
}

NACKED int delay_ticks(uint32_t ticks)
{
    SYSCALL(DELAY_TICKS);
}

unsigned int sleep(unsigned int seconds)
{
    if (seconds == 0)
        return 0;

    uint64_t remaining = seconds;
    const uint64_t max_seconds = UINT32_MAX / OS_TICK_FREQ;

    while (remaining > 0) {
        uint64_t chunk = remaining;
        if (chunk > max_seconds)
            chunk = max_seconds;

        delay_ticks((uint32_t) (chunk * OS_TICK_FREQ));
        remaining -= chunk;
    }

    return 0;
}

int usleep(useconds_t usec)
{
    if (usec == 0)
        return 0;

    if (usec >= 1000000)
        return -EINVAL;

    uint64_t ticks =
        (OS_TICK_FREQ * (uint64_t) usec + 1000000ULL - 1) / 1000000ULL;

    if (ticks == 0)
        ticks = 1;

    delay_ticks((uint32_t) ticks);
    return 0;
}
