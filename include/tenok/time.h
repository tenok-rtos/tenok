#ifndef __TIME_H__
#define __TIME_H__

#include <stdint.h>
#include <signal.h>
#include <sys/types.h>

#define CLOCK_REALTIME  0
#define CLOCK_MONOTONIC 1

struct timespec {
    time_t tv_sec;  /* seconds */
    long   tv_nsec; /* nanoseconds */
};

struct itimerspec {
    struct timespec it_value;
    struct timespec it_interval;
};

int clock_getres(clockid_t clk_id, struct timespec *res);
int clock_gettime(clockid_t clockid, struct timespec *tp);
int clock_settime(clockid_t clk_id, const struct timespec *tp);

int timer_create(clockid_t clockid, struct sigevent *sevp,
                 timer_t *timerid);
int timer_delete(timer_t timerid);
int timer_settime(timer_t timerid, int flags,
                  const struct itimerspec *new_value,
                  struct itimerspec *old_value);
int timer_gettime(timer_t timerid, struct itimerspec *curr_value);

#endif
