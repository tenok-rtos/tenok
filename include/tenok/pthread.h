/**
 * @file
 */
#ifndef __PTHREAD_H__
#define __PTHREAD_H__

#include <stdbool.h>
#include <stdint.h>
#include <sys/sched.h>

#include <common/list.h>

#define PTHREAD_ONCE_INIT                                     \
    {                                                         \
        .wq = {.next = NULL, .prev = NULL}, .finished = false \
    }

#define PTHREAD_CREATE_DETACHED 0
#define PTHREAD_CREATE_JOINABLE 1

#define PTHREAD_PRIO_NONE 0
#define PTHREAD_PRIO_INHERIT 1

#define __SIZEOF_PTHREAD_MUTEXATTR_T 4 /* sizeof(struct mutex_attr) */
#define __SIZEOF_PTHREAD_MUTEX_T 16    /* sizeof(struct mutex) */
#define __SIZEOF_PTHREAD_ATTR_T 20     /* sizeof(struct thread_attr) */
#define __SIZEOF_PTHREAD_COND_T 8      /* sizeof(struct cond) */
#define __SIZEOF_PTHREAD_ONCE_T 12     /* sizeof(struct thread_once) */

typedef uint32_t pthread_t;
typedef uint32_t pthread_condattr_t;

typedef union {
    char __size[__SIZEOF_PTHREAD_MUTEXATTR_T];
    uint32_t __align;
} pthread_mutexattr_t;

typedef union {
    char __size[__SIZEOF_PTHREAD_MUTEX_T];
    uint32_t __align;
} pthread_mutex_t;

typedef union {
    char __size[__SIZEOF_PTHREAD_ATTR_T];
    uint32_t __align;
} pthread_attr_t;

typedef union {
    char __size[__SIZEOF_PTHREAD_COND_T];
    uint32_t __align;
} pthread_cond_t;

typedef union {
    char __size[__SIZEOF_PTHREAD_ONCE_T];
    uint32_t __align;
} pthread_once_t;

/**
 * @brief  Initialize a thread attribute object with default values
 * @param  attr: The attribute object to initialize.
 * @retval int: 0 on sucess and nonzero error number on error.
 */
int pthread_attr_init(pthread_attr_t *attr);

/**
 * @brief  Destroy the given thread attribute object
 * @param  attr: The thread attribute object to destroy.
 * @retval int: 0 on sucess and nonzero error number on error.
 */
int pthread_attr_destroy(pthread_attr_t *attr);

/**
 * @brief  Set scheduling parameter of a thread attriute object
 * @param  attr: The attribute object to set.
 * @param  param: The scheduling parameter for setting the attribute object.
 * @retval int: 0 on sucess and nonzero error number on error.
 */
int pthread_attr_setschedparam(pthread_attr_t *attr,
                               const struct sched_param *param);

/**
 * @brief  Get scheduling parameter of a thread attriute object
 * @param  attr: The attribute object to retrieve the scheduling parameter.
 * @param  param: For returning the scheduling parameter.
 * @retval int: 0 on sucess and nonzero error number on error.
 */
int pthread_attr_getschedparam(const pthread_attr_t *attr,
                               struct sched_param *param);

/**
 * @brief  Set scheduling policy of a thread attriute object
 * @param  attr: The attribute object to set.
 * @param  param: The scheduling policy for setting the attribute object.
 * @retval int: 0 on sucess and nonzero error number on error.
 */
int pthread_attr_setschedpolicy(pthread_attr_t *attr, int policy);

/**
 * @brief  Get scheduling parameter of a thread attriute object
 * @param  attr: The attribute object to retrieve the scheduling policy.
 * @param  param: For returning the scheduling policy.
 * @retval int: 0 on sucess and nonzero error number on error.
 */
int pthread_attr_getschedpolicy(const pthread_attr_t *attr, int *policy);

/**
 * @brief  Set priority inheritance protocol of a mutex attriute object
 * @param  attr: The attribute object to set.
 * @param  param: The protocol to set the attribute object.
 * @retval int: 0 on sucess and nonzero error number on error.
 */
int pthread_mutexattr_setprotocol(pthread_mutexattr_t *attr, int protocol);

/**
 * @brief  Get priority inheritance protocol of a mutex attriute object
 * @param  attr: The attribute object to retrieve the priority inheritance
 *               protocol.
 * @param  param: For returning the priority inheritance protocol.
 * @retval int: 0 on sucess and nonzero error number on error.
 */
int pthread_mutexattr_getprotocol(const pthread_mutexattr_t *attr,
                                  int *protocol);

/**
 * @brief  Set stack size parameter of a thread attriute object
 * @param  attr: The attribute object to set.
 * @param  param: The stack size for setting the attribute object.
 * @retval int: 0 on sucess and nonzero error number on error.
 */
int pthread_attr_setstacksize(pthread_attr_t *attr, size_t stacksize);

/**
 * @brief  Get stack size parameter of a thread attriute object
 * @param  attr: The attribute object to retrieve stack size.
 * @param  stacksize: For returning the stack size from the attribute object.
 * @retval int: 0 on sucess and nonzero error number on error.
 */
int pthread_attr_getstacksize(const pthread_attr_t *attr, size_t *stacksize);

/**
 * @brief  Set detach state parameter of a thread attriute object
 * @param  attr: The attribute object to set detach state.
 * @param  detachstate: The detach state for setting the attribute object.
 * @retval int: 0 on sucess and nonzero error number on error.
 */
int pthread_attr_setdetachstate(pthread_attr_t *attr, int detachstate);

/**
 * @brief  Get detach state parameter of a thread attriute object
 * @param  attr: The attribute object to retrieve detach state.
 * @param  detachstate: For returning the detach state from the attribute
 *          object.
 * @retval int: 0 on sucess and nonzero error number on error.
 */
int pthread_attr_getdetachstate(const pthread_attr_t *attr, int *detachstate);

/**
 * @brief  Set stack address parameter of a thread attriute object
 * @param  attr: The attribute object to set stack address.
 * @param  detachstate: The stack address for setting the attribute object.
 * @retval int: 0 on sucess and nonzero error number on error.
 */
int pthread_attr_setstackaddr(pthread_attr_t *attr, void *stackaddr);

/**
 * @brief  Get stack address parameter of a thread attriute object
 * @param  attr: The attribute object to retrieve stack address.
 * @param  detachstate: For returning the stack address from the attribute
 *         object.
 * @retval int: 0 on sucess and nonzero error number on error.
 */
int pthread_attr_getstackaddr(const pthread_attr_t *attr, void **stackaddr);

/**
 * @brief  Start a new thread in the calling task
 * @param  thread: The thread ID of the new thread to return.
 * @param  attr: The attribute object for configuring the new thread.
 * @param  start_routine: The function to be called after the thread is
 *         launched.
 * @param  arg: The sole argument of the start_routine to be provided (Not
 * implemented yet).
 * @retval int: 0 on sucess and nonzero error number on error.
 */
int pthread_create(pthread_t *thread,
                   const pthread_attr_t *attr,
                   void *(*start_routine)(void *),
                   void *arg);

/**
 * @brief  Return the thread ID of the calling thread
 * @param  None
 * @retval pthread_t: The hread ID.
 */
pthread_t pthread_self(void);

/**
 * @brief  Wait for a thread specified with the thread ID to terminate
 * @param  thread: The thread ID to be provided.
 * @param  retval: The pointer to the return value pointer to provide.
 * @retval int: 0 on sucess and nonzero error number on error.
 */
int pthread_join(pthread_t thread, void **retval);

/**
 * @brief  Mark the thread identified by thread as detached. When a detached
 *         thread terminates, its resources are automatically released back to
 *         the  system without the need for another thread to join with the
 *         terminated thread
 * @param  thread: The thread ID to be provided.
 * @retval int: 0 on sucess and nonzero error number on error.
 */
int pthread_detach(pthread_t thread);

/**
 * @brief  Send a cancellation request to a thread specified with the thread ID
 * @param  thread: The thread ID to provide.
 * @retval int: 0 on sucess and nonzero error number on error.
 */
int pthread_cancel(pthread_t thread);

/**
 * @brief  Compare thread IDs
 * @param  t1: The first thread ID to be provided.
 * @param  t2: The second thread ID to be provided.
 * @retval int: If the two thread IDs are equal, the function returns a nonzero
 *              value; otherwise, it returns 0.
 */
int pthread_equal(pthread_t t1, pthread_t t2);

/**
 * @brief  Set the scheduling parameters of a thread specified with the thread
 *         ID
 * @param  thread: Thread ID to provide.
 * @param  policy: Not used.
 * @param  param: The scheduling parameter to set the thread.
 * @retval int: 0 on sucess and nonzero error number on error.
 */
int pthread_setschedparam(pthread_t thread,
                          int policy,
                          const struct sched_param *param);

/**
 * @brief  Get the scheduling parameters of a thread specified with the thread
 *         ID
 * @param  thread: The hread ID to provide.
 * @param  policy: Not used.
 * @param  param: For returning the scheduling parameter from the thread.
 * @retval int: 0 on sucess and nonzero error number on error.
 */
int pthread_getschedparam(pthread_t thread,
                          int *policy,
                          struct sched_param *param);

/**
 * @brief  Cause the calling thread to relinquish the CPU
 * @param  None
 * @retval int: 0 on sucess and nonzero error number on error.
 */
int pthread_yield(void);

/**
 * @brief  Send a signal to a thread specified with the thread ID
 * @param  thread: The thread ID to provide.
 * @param  sig: The signal number to provide.
 * @retval int: 0 on sucess and nonzero error number on error.
 */
int pthread_kill(pthread_t thread, int sig);

/**
 * @brief  Terminate the calling thread and returns a value via retval
 * @param  retval: The return value pointer to provide.
 * @retval None
 */
void pthread_exit(void *retval);

/**
 * @brief  Initialize the given mutex attribute object with default values
 * @param  attr: The mutex attribute object to initialize.
 * @retval int: 0 on sucess and nonzero error number on error.
 */
int pthread_mutexattr_init(pthread_mutexattr_t *attr);

/**
 * @brief  Destroy the given mutex attribute object
 * @param  attr: The mutex attribute object to destroy.
 * @retval int: 0 on sucess and nonzero error number on error.
 */
int pthread_mutexattr_destroy(pthread_mutexattr_t *attr);


/**
 * @brief  Initialize a mutex with specified attributes
 * @param  mutex: The mutex object to be initialized.
 * @param  attr: The attribute object for initializing the mutex.
 * @retval int: 0 on sucess and nonzero error number on error.
 */
int pthread_mutex_init(pthread_mutex_t *mutex, const pthread_mutexattr_t *attr);

/**
 * @brief  Destroy the given mutex object
 * @param  mutex: The mutex object to destroy.
 * @retval int: 0 on sucess and nonzero error number on error.
 */
int pthread_mutex_destroy(pthread_mutex_t *mutex);

/**
 * @brief  Unlock the given mutex
 * @param  mutex: The mutex object to unlock.
 * @retval int: 0 on sucess and nonzero error number on error.
 */
int pthread_mutex_unlock(pthread_mutex_t *mutex);

/**
 * @brief  Lock the given mutex
 * @param  mutex: The mutex object to lock.
 * @retval int: 0 on sucess and nonzero error number on error.
 */
int pthread_mutex_lock(pthread_mutex_t *mutex);

/**
 * @brief  Lock the given mutex. If the mutex object referenced by mutex is
           currently locked (by any thread, including the current thread),
           the call shall return immediately
 * @param  mutex: The mutex object to lock.
 * @retval int: 0 on sucess and nonzero error number on error.
 */
int pthread_mutex_trylock(pthread_mutex_t *mutex);

/**
 * @brief  Initialize the given attribute object of conditional variable with
 *         default values
 * @param  attr: The attribute object of conditional variable to initialize.
 * @retval int: 0 on sucess and nonzero error number on error.
 */
int pthread_condattr_init(pthread_condattr_t *attr);

/**
 * @brief  Destroy the given attribute object of conditional variable
 * @param  attr: The attribute object of conditional variable to destroy.
 * @retval int: 0 on sucess and nonzero error number on error.
 */
int pthread_condattr_destroy(pthread_condattr_t *attr);

/**
 * @brief  Initializes a conditional variable with specified attributes
 * @param  cond: The conditional variable object to be initialized.
 * @param  cond_attr: The attribute object for initializing the conditional
 *         variable.
 * @retval int: 0 on sucess and nonzero error number on error.
 */
int pthread_cond_init(pthread_cond_t *cond, pthread_condattr_t cond_attr);

/**
 * @brief  Destroy the given conditional variable object
 * @param  cond: The conditional variable object to destroy.
 * @retval int: 0 on sucess and nonzero error number on error.
 */
int pthread_cond_destroy(pthread_cond_t *cond);

/**
 * @brief  Restart one of the threads that are waiting on the condition variable
 * @param  cond: The conditional variable object to signal the state change.
 * @retval int: 0 on sucess and nonzero error number on error.
 */
int pthread_cond_signal(pthread_cond_t *cond);

/**
 * @brief  Restart all of the threads that are waiting on the condition variable
 * @param  cond: The conditional variable object to signal the state change.
 * @retval int: 0 on sucess and nonzero error number on error.
 */
int pthread_cond_broadcast(pthread_cond_t *cond);

/**
 * @brief  Atomically unlocks the mutex and waits for the condition variable to
 *         be signaled
 * @param  cond: The sonditional variable object for waiting the state change.
 * @retval int: 0 on sucess and nonzero error number on error.
 */
int pthread_cond_wait(pthread_cond_t *cond, pthread_mutex_t *mutex);

/**
 * @brief  To ensure that a piece of initialization code is executed at most
 *         once
 * @param  once_control: The object to track the execution state of the
 * init_routine.
 * @param  init_routine: The function to be called for once.
 * @retval int: 0 on sucess and nonzero error number on error.
 */
int pthread_once(pthread_once_t *once_control, void (*init_routine)(void));

#endif
