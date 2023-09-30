/**
 * @file
 */
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

/**
 * @brief  Get the resolution (precision) of the specified clock clockid
 * @param  clk_id:  The clock ID to provide.
 * @param  res: The time object for returning the clock resolution.
 * @retval int: The write number on sucess and nonzero error number on error.
 */
int clock_getres(clockid_t clk_id, struct timespec *res);

/**
 * @brief  Retrieve the time of the specified clock clockid
 * @param  clk_id: The clock ID to provide.
 * @param  tp: The time object for returning the time.
 * @retval int: The write number on sucess and nonzero error number on error.
 */
int clock_gettime(clockid_t clockid, struct timespec *tp);

/**
 * @brief  Set the time of the specified clock clockid
 * @param  clk_id: The clock ID to provide.
 * @param  tp: The time object for setting the clock.
 * @retval int: The write number on sucess and nonzero error number on error.
 */
int clock_settime(clockid_t clk_id, const struct timespec *tp);

/**
 * @brief  Create a new per-thread interval timer
 * @param  clk_id: The clock ID to provide.
 * @param  sevp: Signal event when timr expires to provide.
 * @param  timerid: For returning the new created timer's ID.
 * @retval int: The write number on sucess and nonzero error number on error.
 */
int timer_create(clockid_t clockid, struct sigevent *sevp,
                 timer_t *timerid);

/**
 * @brief  Delete the timer whose ID is given in timerid
 * @param  timerid: The timer ID to provide.
 * @retval int: The write number on sucess and nonzero error number on error.
 */
int timer_delete(timer_t timerid);

/**
 * @brief  Arm or disarm the timer identified by timerid
 * @param  timerid:  The timer ID to provide.
 * @param  flags: Not used.
 * @param  new_value: The timer object for configuring new timer setting.
 * @param  old_value: The timer object for storing the old timer setting.
 * @retval int: The write number on sucess and nonzero error number on error.
 */
int timer_settime(timer_t timerid, int flags,
                  const struct itimerspec *new_value,
                  struct itimerspec *old_value);

/**
 * @brief  Return the time until next expiration, and the  interval, for the timer
           specified by timerid
 * @param  timerid:  The timer ID to provide.
 * @param  curr_value: The timer object for returning the timer time.
 * @retval int: The write number on sucess and nonzero error number on error.
 */
int timer_gettime(timer_t timerid, struct itimerspec *curr_value);

#endif
