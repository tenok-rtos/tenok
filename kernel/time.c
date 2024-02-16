#include <time.h>

#include <arch/port.h>
#include <kernel/syscall.h>
#include <kernel/time.h>

#include "kconfig.h"

#define NANOSECOND_TICKS (1000000000 / OS_TICK_FREQ)

static struct timespec sys_time;

void timer_up_count(struct timespec *time)
{
    time->tv_nsec += NANOSECOND_TICKS;

    if (time->tv_nsec >= 1000000000) {
        time->tv_sec++;
        time->tv_nsec -= 1000000000;
    }
}

void timer_down_count(struct timespec *time)
{
    time->tv_nsec -= NANOSECOND_TICKS;

    if (time->tv_nsec < 0 && time->tv_sec > 0) {
        time->tv_sec--;
        time->tv_nsec += 1000000000;
    } else if (time->tv_nsec <= 0 && time->tv_sec <= 0) {
        time->tv_sec = 0;
        time->tv_nsec = 0;
    }
}

void time_add(struct timespec *time, time_t sec, long nsec)
{
    time->tv_sec += sec;
    time->tv_nsec += nsec;

    if (time->tv_nsec >= 1000000000) {
        time->tv_sec++;
        time->tv_nsec -= 1000000000;
    }
}

void system_timer_update(void)
{
    timer_up_count(&sys_time);
}

void get_sys_time(struct timespec *tp)
{
    *tp = sys_time;
}

void set_sys_time(const struct timespec *tp)
{
    sys_time = *tp;
}

int clock_getres(clockid_t clockid, struct timespec *res)
{
    // TODO: Check clock ID

    res->tv_sec = 0;
    res->tv_nsec = NANOSECOND_TICKS;

    return 0;
}

NACKED int clock_gettime(clockid_t clockid, struct timespec *tp)
{
    SYSCALL(CLOCK_GETTIME);
}

NACKED int clock_settime(clockid_t clk_id, const struct timespec *tp)
{
    SYSCALL(CLOCK_SETTIME);
}

NACKED int timer_create(clockid_t clockid,
                        struct sigevent *sevp,
                        timer_t *timerid)
{
    SYSCALL(TIMER_CREATE);
}

NACKED int timer_delete(timer_t timerid)
{
    SYSCALL(TIMER_DELETE);
}

NACKED int timer_settime(timer_t timerid,
                         int flags,
                         const struct itimerspec *new_value,
                         struct itimerspec *old_value)
{
    SYSCALL(TIMER_SETTIME);
}

NACKED int timer_gettime(timer_t timerid, struct itimerspec *curr_value)
{
    SYSCALL(TIMER_GETTIME);
}

time_t time(time_t *tloc)
{
    struct timespec tp;
    clock_gettime(CLOCK_MONOTONIC, &tp);

    if (tloc)
        *tloc = tp.tv_sec;

    return tp.tv_sec;
}

void msleep(unsigned int msecs)
{
    // TODO
}

ktime_t ktime_get(void)
{
    return sys_time.tv_sec * 1000 + sys_time.tv_nsec / 1000000;
}
