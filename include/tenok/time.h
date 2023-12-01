/**
 * @file
 */
#ifndef __TIME_H__
#define __TIME_H__

#include <signal.h>
#include <stdint.h>
#include <sys/types.h>

#define CLOCK_REALTIME 0
#define CLOCK_MONOTONIC 1

struct timespec {
    time_t tv_sec; /* Seconds */
    long tv_nsec;  /* Nanoseconds */
};

struct itimerspec {
    struct timespec it_value;
    struct timespec it_interval;
};

/**
 * @brief  Get the resolution (precision) of the specified clock ID
 * @param  clk_id:  The clock ID to provide.
 * @param  res: The time object for returning the clock resolution.
 * @retval int: 0 on success and nonzero error number on error.
 */
int clock_getres(clockid_t clockid, struct timespec *res);

/**
 * @brief  Retrieve the time of the specified clock ID
 * @param  clk_id: The clock ID to provide.
 * @param  tp: The time object for returning the time.
 * @retval int: 0 on success and nonzero error number on error.
 */
int clock_gettime(clockid_t clockid, struct timespec *tp);

/**
 * @brief  Set the time of the specified clock ID
 * @param  clk_id: The clock ID to provide.
 * @param  tp: The time object for setting the clock.
 * @retval int: 0 on success and nonzero error number on error.
 */
int clock_settime(clockid_t clockid, const struct timespec *tp);

/**
 * @brief  Create a new per-thread interval timer
 * @param  clk_id: The clock ID to provide.
 * @param  sevp: For signaling an event when the timer expired.
 * @param  timerid: For returning the ID of the newly created timer.
 * @retval int: 0 on success and nonzero error number on error.
 */
int timer_create(clockid_t clockid, struct sigevent *sevp, timer_t *timerid);

/**
 * @brief  Delete the timer specified with the timer ID
 * @param  timerid: The timer ID to provide.
 * @retval int: 0 on success and nonzero error number on error.
 */
int timer_delete(timer_t timerid);

/**
 * @brief  Arm or disarm the timer specified by the timer ID
 * @param  timerid: The timer ID to provide.
 * @param  flags: Not used.
 * @param  new_value: Pointer the the new timer setting.
 * @param  old_value: Pointer to the memory space for storing the old timer
 *         setting.
 * @retval int: 0 on success and nonzero error number on error.
 */
int timer_settime(timer_t timerid,
                  int flags,
                  const struct itimerspec *new_value,
                  struct itimerspec *old_value);

/**
 * @brief  Return the timer interval and the time until next expiration
 * @param  timerid:  The timer ID to provide.
 * @param  curr_value: The timer object for returning the timer time.
 * @retval int: 0 on success and nonzero error number on error.
 */
int timer_gettime(timer_t timerid, struct itimerspec *curr_value);

#endif
