/**
 * @file
 */
#ifndef __SCHED_H__
#define __SCHED_H__

/**
 * @brief  Return the maximum priority of the thread can be set
 * @param  policy: Not used.
 * @retval int: The maximum priority of the thread can be set.
 */
int sched_get_priority_max(int policy);

/**
 * @brief  Return the minimum priority of the thread can be set
 * @param  policy: Not used.
 * @retval int: The minimum priority of the thread can be set.
 */
int sched_get_priority_min(int policy);

/**
 * @brief  Cause the calling thread to relinquish the CPU
 * @retval int: 0 on sucess and nonzero error number on error.
 */
int sched_yield(void);

#endif
