/**
 * @file
 */
#ifndef __KERNEL_MUTEX_H__
#define __KERNEL_MUTEX_H__

#include <stdbool.h>

#include <kernel/list.h>

struct mutex {
    struct thread_info *owner;
    struct list wait_list;
};

/**
 * @brief  Initialize the given mutex. The function should only
           be called inside the kernel space
 * @param  mtx: Mutex object to provide.
 * @retval None
 */
void mutex_init(struct mutex *mtx);

/**
 * @brief  Returns true if the mutex is locker, otherwise returns
 *         false. The function should only be called inside the
 *         kernel space
 * @param  mtx: Mutex object to be queried.
 * @retval true if the mutex is locked, false if unlocked.
 */
bool mutex_is_locked(struct mutex *mtx);

/**
 * @brief  Lock the given mutex. The function should only be
 *         called inside the kernel space
 * @param  mtx: Mutex object to provide.
 * @retval int: 0 on sucess and nonzero error number on error.
 */
int mutex_lock(struct mutex *mtx);

/**
 * @brief  Unlock the given mutex. The function should only be
 *         called inside the kernel space
 * @param  mtx: Mutex object to provide.
 * @retval int: 0 on sucess and nonzero error number on error.
 */
int mutex_unlock(struct mutex *mtx);

#endif
