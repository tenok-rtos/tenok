/* Newlib's own would redefine struct timespec, which time.h already has */
#ifndef _TENOK_SYS_TIME_H
#define _TENOK_SYS_TIME_H

#define _SYS__TIMESPEC_H_ /* keep newlib from defining struct timespec */

#include <sys/types.h>
#include <time.h>

struct timeval {
    time_t tv_sec;
    long tv_usec;
};

struct timezone {
    int tz_minuteswest;
    int tz_dsttime;
};

int gettimeofday(struct timeval *tv, void *tz);
int settimeofday(const struct timeval *tv, const struct timezone *tz);
int utimes(const char *path, const struct timeval times[2]);

#endif
