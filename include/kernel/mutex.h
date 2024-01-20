/**
 * @file
 */
#ifndef __KERNEL_MUTEX_H__
#define __KERNEL_MUTEX_H__

#include <stdbool.h>

#include <common/list.h>

struct mutex_attr {
    int protocol;
};

struct mutex {
    int protocol;
    struct thread_info *owner;
    struct list_head wait_list;
};

struct cond {
    struct list_head task_wait_list;
};

/**
 * @brief  Initialize the mutex.
 * @param  mtx: Pointer to the mutex.
 * @retval None
 */
void mutex_init(struct mutex *mtx);

/**
 * @brief  Check if the mutex is locked or not
 * @param  mtx: Pointer to the mutex.
 * @retval bool: true or false.
 */
bool mutex_is_locked(struct mutex *mtx);

/**
 * @brief  Lock the mutex
 * @param  mtx: Pointer to the mutex.
 * @retval int: 0 on success and nonzero error number on error.
 */
int mutex_lock(struct mutex *mtx);

int __mutex_lock(struct mutex *mtx);

/**
 * @brief  Unlock the mutex.
 * @param  mtx: Pointer to the mutex.
 * @retval int: 0 on success and nonzero error number on error.
 */
int mutex_unlock(struct mutex *mtx);

#endif
