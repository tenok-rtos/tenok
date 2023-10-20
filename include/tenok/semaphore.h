/**
 * @file
 */
#ifndef __SEMAPHORE_H__
#define __SEMAPHORE_H__

#include <kernel/list.h>
#include <kernel/spinlock.h>
#include <kernel/semaphore.h>

typedef struct semaphore sem_t;

/**
 * @brief  Initialize the semaphore at the address pointed to by sem
 * @param  sem: The semaphore object to initialize.
 * @param  pshared: Not used.
 * @param  value: The value specifies the initial value for the semaphore.
 * @retval int: 0 on sucess and nonzero error number on error.
 */
int sem_init(sem_t *sem, int pshared, unsigned int value);

/**
 * @brief  Destroy the given semaphore object
 * @param  sem: The semaphore object to destroy.
 * @retval int: 0 on sucess and nonzero error number on error.
 */
int sem_destroy(sem_t *sem);

/**
 * @brief  Increments (unlocks) the semaphore pointed to by sem
 * @param  sem: The semaphore object to provide.
 * @retval int: 0 on sucess and nonzero error number on error.
 */
int sem_post(sem_t *sem);

/**
 * @brief  The same as sem_wait(), except that if the decrement cannot
 *         be immediately performed, then call returns an error instead
 *         of blocking.
 * @param  sem: The semaphore object to provide.
 * @retval int: 0 on sucess and nonzero error number on error.
 */
int sem_trywait(sem_t *sem);

/**
 * @brief  Decrement (locks) the semaphore pointed to by sem. If the semaphore
 *         currently has the value zero, then the call blocks until it becomes
 *         possible to  perform the decrement.
 * @param  sem: The semaphore object to provide.
 * @retval int: 0 on sucess and nonzero error number on error.
 */
int sem_wait(sem_t *sem);

/**
 * @brief  Place the current value of the semaphore pointed to sem into the
 *         integer pointed to by sval
 * @param  sem: The semaphore object to provide.
 * @param  sval: For returning the current value of the semaphore.
 * @retval int: 0 on sucess and nonzero error number on error.
 */
int sem_getvalue(sem_t *sem, int *sval);

#endif
