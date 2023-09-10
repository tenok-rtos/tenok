#ifndef __TIME_H__
#define __TIME_H__

#include <stdint.h>

#include <tenok/sys/types.h>

#define CLOCK_MONOTONIC 0

#define timespec tenok__timespec

struct tenok__timespec {
    time_t tv_sec;  //seconds
    long   tv_nsec; //nanoseconds
};

void sys_time_update_handler(void);

int clock_gettime(clockid_t clockid, struct timespec *tp);

#endif
