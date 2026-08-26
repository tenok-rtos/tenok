#include <errno.h>
#include <tenok.h>
#include <time.h>

#include <arch/port.h>
#include <kernel/errno.h>
#include <kernel/syscall.h>
#include <kernel/time.h>

#include "kconfig.h"

#define NANOSECOND_TICKS (1000000000 / OS_TICK_FREQ)

/* Time elapsed since the system was booted */
static struct timespec sys_time;

/* Difference between the wall clock and the monotonic clock. Tenok has no
 * battery backed RTC, so the wall clock starts at the Unix epoch until it is
 * set with clock_settime().
 */
static struct timespec realtime_offset;

static void normalize_timespec(struct timespec *time)
{
    if (time->tv_nsec >= 1000000000L || time->tv_nsec <= -1000000000L) {
        time->tv_sec += time->tv_nsec / 1000000000L;
        time->tv_nsec %= 1000000000L;
    }

    if (time->tv_nsec < 0) {
        time->tv_sec -= 1;
        time->tv_nsec += 1000000000L;
    }
}

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

    normalize_timespec(time);
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

void get_realtime(struct timespec *tp)
{
    tp->tv_sec = sys_time.tv_sec + realtime_offset.tv_sec;
    tp->tv_nsec = sys_time.tv_nsec + realtime_offset.tv_nsec;

    normalize_timespec(tp);
}

void set_realtime(const struct timespec *tp)
{
    realtime_offset.tv_sec = tp->tv_sec - sys_time.tv_sec;
    realtime_offset.tv_nsec = tp->tv_nsec - sys_time.tv_nsec;

    normalize_timespec(&realtime_offset);
}

int clock_getres(clockid_t clockid, struct timespec *res)
{
    // TODO: Check clock ID

    if (!res)
        return -EINVAL;

    if (clockid != CLOCK_MONOTONIC && clockid != CLOCK_REALTIME)
        return -EINVAL;

    res->tv_sec = 0;
    res->tv_nsec = NANOSECOND_TICKS;

    return 0;
}

static NACKED int __clock_gettime(clockid_t clockid, struct timespec *tp)
{
    SYSCALL(CLOCK_GETTIME);
}

int clock_gettime(clockid_t clockid, struct timespec *tp)
{
    return set_errno(__clock_gettime(clockid, tp));
}

static NACKED int __clock_settime(clockid_t clk_id, const struct timespec *tp)
{
    SYSCALL(CLOCK_SETTIME);
}

int clock_settime(clockid_t clk_id, const struct timespec *tp)
{
    return set_errno(__clock_settime(clk_id, tp));
}

static NACKED int __timer_create(clockid_t clockid,
                                 struct sigevent *sevp,
                                 timer_t *timerid)
{
    SYSCALL(TIMER_CREATE);
}

int timer_create(clockid_t clockid, struct sigevent *sevp, timer_t *timerid)
{
    return set_errno(__timer_create(clockid, sevp, timerid));
}

static NACKED int __timer_delete(timer_t timerid)
{
    SYSCALL(TIMER_DELETE);
}

int timer_delete(timer_t timerid)
{
    return set_errno(__timer_delete(timerid));
}

static NACKED int __timer_settime(timer_t timerid,
                                  int flags,
                                  const struct itimerspec *new_value,
                                  struct itimerspec *old_value)
{
    SYSCALL(TIMER_SETTIME);
}

int timer_settime(timer_t timerid,
                  int flags,
                  const struct itimerspec *new_value,
                  struct itimerspec *old_value)
{
    return set_errno(__timer_settime(timerid, flags, new_value, old_value));
}

static NACKED int __timer_gettime(timer_t timerid,
                                  struct itimerspec *curr_value)
{
    SYSCALL(TIMER_GETTIME);
}

int timer_gettime(timer_t timerid, struct itimerspec *curr_value)
{
    return set_errno(__timer_gettime(timerid, curr_value));
}

time_t time(time_t *tloc)
{
    struct timespec tp;
    clock_gettime(CLOCK_REALTIME, &tp);

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

static bool timespec_valid(const struct timespec *ts)
{
    return ts && ts->tv_sec >= 0 && ts->tv_nsec >= 0 &&
           ts->tv_nsec < 1000000000L;
}

static int timespec_cmp(const struct timespec *a, const struct timespec *b)
{
    if (a->tv_sec == b->tv_sec) {
        if (a->tv_nsec == b->tv_nsec)
            return 0;
        return (a->tv_nsec > b->tv_nsec) ? 1 : -1;
    }
    return (a->tv_sec > b->tv_sec) ? 1 : -1;
}

static void timespec_sub(struct timespec *out,
                         const struct timespec *a,
                         const struct timespec *b)
{
    out->tv_sec = a->tv_sec - b->tv_sec;
    out->tv_nsec = a->tv_nsec - b->tv_nsec;
    if (out->tv_nsec < 0) {
        out->tv_sec -= 1;
        out->tv_nsec += 1000000000L;
    }
    if (out->tv_sec < 0) {
        out->tv_sec = 0;
        out->tv_nsec = 0;
    }
}

static uint64_t timespec_to_ticks(const struct timespec *ts)
{
    uint64_t ns =
        (uint64_t) ts->tv_sec * 1000000000ULL + (uint64_t) ts->tv_nsec;
    uint64_t tick_ns = 1000000000ULL / OS_TICK_FREQ;
    if (tick_ns == 0)
        return 0;
    return (ns + tick_ns - 1) / tick_ns;
}

int clock_nanosleep(clockid_t clockid,
                    int flags,
                    const struct timespec *req,
                    struct timespec *rem)
{
    if (!timespec_valid(req))
        return -EINVAL;

    if (clockid != CLOCK_MONOTONIC)
        return -EINVAL;

    if (flags != 0 && flags != TIMER_ABSTIME)
        return -EINVAL;

    struct timespec duration = *req;

    if (flags == TIMER_ABSTIME) {
        struct timespec now;
        get_sys_time(&now);
        if (timespec_cmp(req, &now) <= 0) {
            if (rem) {
                rem->tv_sec = 0;
                rem->tv_nsec = 0;
            }
            return 0;
        }
        timespec_sub(&duration, req, &now);
    }

    uint64_t ticks = timespec_to_ticks(&duration);
    if (ticks > 0)
        delay_ticks((uint32_t) ticks);

    if (rem) {
        rem->tv_sec = 0;
        rem->tv_nsec = 0;
    }

    return 0;
}

int nanosleep(const struct timespec *req, struct timespec *rem)
{
    return clock_nanosleep(CLOCK_MONOTONIC, 0, req, rem);
}
