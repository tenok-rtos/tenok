/**
 * @file
 */
#ifndef __SEMAPHORE_H__
#define __SEMAPHORE_H__

#include <stdint.h>

#define __SIZEOF_SEM_T 12 /* sizeof(struct semaphore) */

typedef union {
    char __size[__SIZEOF_SEM_T];
    uint32_t __align;
} sem_t;

/**
 * @brief  Initialize the semaphore
 * @param  sem: Pointer to the semaphore.
 * @param  pshared: Not used.
 * @param  value: The initial value of the semaphore.
 * @retval int: 0 on success and nonzero error number on error.
 */
int sem_init(sem_t *sem, int pshared, unsigned int value);

/**
 * @brief  Destroy the semaphore
 * @param  sem: Pointer to the semaphore.
 * @retval int: 0 on success and nonzero error number on error.
 */
int sem_destroy(sem_t *sem);

/**
 * @brief  Increase the value of the semaphore
 * @param  sem: Pointer to the semaphore.
 * @retval int: 0 on success and nonzero error number on error.
 */
int sem_post(sem_t *sem);

/**
 * @brief  The same as sem_wait(), except that if the decrement cannot
 *         be immediately performed, then the function returns an error
 *         instead of blocking
 * @param  sem: Pointer to the semaphore.
 * @retval int: 0 on success and nonzero error number on error.
 */
int sem_trywait(sem_t *sem);

/**
 * @brief  Decrement the value of the semaphore. If the semaphore currently
 *         has the value zero, then the function blocks until it becomes
 *         possible to  perform the decrement.
 * @param  sem: Pointer to the semaphore.
 * @retval int: 0 on success and nonzero error number on error.
 */
int sem_wait(sem_t *sem);

/**
 * @brief  Get the value of the semaphore
 * @param  sem: The semaphore object to provide.
 * @param  sval: For returning current value of the semaphore.
 * @retval int: 0 on success and nonzero error number on error.
 */
int sem_getvalue(sem_t *sem, int *sval);

#endif
