/**
 * @file
 */
#ifndef __KERNEL_SEMAPHORE_H__
#define __KERNEL_SEMAPHORE_H__

#include <stdint.h>

#include <common/list.h>

struct semaphore {
    int32_t count;
    struct list_head wait_list;
};

/**
 * @brief  Initialize the semaphore
 * @param  sem: Pointer to the semaphore.
 * @param  val: Initial value of the semaphore.
 * @retval None
 */
void sema_init(struct semaphore *sem, int val);

/**
 * @brief  Decrease the number of the semaphore
 * @param  sem: Pointer to the semaphore.
 * @retval int: 0 on success and nonzero error number on error.
 */
int down(struct semaphore *sem);

/**
 * @brief  The same as down(), except that if the decrement cannot be
 *         immediately performed, then call returns an error instead
 *         of blocking
 * @param  sem: Pointer to the semaphore.
 * @retval int: 0 on success and nonzero error number on error.
 */
int down_trylock(struct semaphore *sem);

/**
 * @brief  Increase the number of the semaphore
 * @param  sem: Pointer to the semaphore.
 * @retval int: 0 on success and nonzero error number on error.
 */
int up(struct semaphore *sem);

#endif
