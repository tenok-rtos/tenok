/**
 * @file
 */
#ifndef __KERNEL_SEMAPHORE_H__
#define __KERNEL_SEMAPHORE_H__

#include <stdint.h>

#include <kernel/list.h>

struct semaphore {
    int32_t count;
    struct list_head wait_list;
};

/**
 * @brief  Initialize the semaphore at the address pointed to by sem.
 *         The function should only be called inside the kernel space
 * @param  sem: The semaphore object to initialize.
 * @param  val: The value specifies the initial value for the semaphore.
 * @retval None
 */
void sema_init(struct semaphore *sem, int val);

/**
 * @brief  Decrements (locks) the semaphore pointed to by sem. The function
 *         should only be called inside the kernel space
 * @param  sem: The semaphore object to provide.
 * @retval int: 0 on sucess and nonzero error number on error.
 */
int down(struct semaphore *sem);

/**
 * @brief  The same as down(), except that if the decrement cannot be
 *         immediately performed, then call returns an error instead
 *         of blocking. The function should only be called inside the
 *         kernel space
 * @param  sem: The semaphore object to provide.
 * @retval int: 0 on sucess and nonzero error number on error.
 */
int down_trylock(struct semaphore *sem);

/**
 * @brief  Increments (unlocks) the semaphore pointed to by sem. The
 *         function should only be called inside the kernel space
 * @param  sem: The semaphore object to provide.
 * @retval int: 0 on sucess and nonzero error number on error.
 */
int up(struct semaphore *sem);

#endif
