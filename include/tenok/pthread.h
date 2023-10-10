/**
 * @file
 */
#ifndef __PTHREAD_H__
#define __PTHREAD_H__

#include <stdint.h>
#include <stdbool.h>
#include <sys/sched.h>

#include <kernel/list.h>
#include <kernel/wait.h>
#include <kernel/mutex.h>
#include <kernel/spinlock.h>

#define PTHREAD_ONCE_INIT \
    {.wq = {.next = NULL, .prev = NULL}, \
     .finished = false}

typedef uint32_t pthread_t;
typedef void pthread_mutex_attr_t;   /* no usage */
typedef uint32_t pthread_condattr_t; /* no usage */
typedef struct mutex pthread_mutex_t;

typedef struct {
    struct sched_param schedparam;
    void *stackaddr;
    size_t stacksize; /* bytes */
} pthread_attr_t;

typedef struct {
    struct list_head task_wait_list;
} pthread_cond_t;

typedef struct {
    wait_queue_head_t wq;
    bool finished;
} pthread_once_t;

/**
 * @brief  Initialize a thread attribute object with default values
 * @param  attr: The attribute object to initialize.
 * @retval int: 0 on sucess and nonzero error number on error.
 */
int pthread_attr_init(pthread_attr_t *attr);

/**
 * @brief  Set scheduling parameter of a thread attriute object
 * @param  attr: The attribute object to set.
 * @param  param: The scheduling parameter for setting the attribute object.
 * @retval int: 0 on sucess and nonzero error number on error.
 */
int pthread_attr_setschedparam(pthread_attr_t *attr, const struct sched_param *param);

/**
 * @brief  Get scheduling parameter of a thread attriute object
 * @param  attr: The attribute object to retrieve the scheduling parameter.
 * @param  param: For returning the scheduling parameter.
 * @retval int: 0 on sucess and nonzero error number on error.
 */
int pthread_attr_getschedparam(const pthread_attr_t *attr, struct sched_param *param);

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
 * @brief  Start a new thread in the calling task
 * @param  thread: The thread ID of the new thread to return.
 * @param  attr: The attribute object for configuring the new thread.
 * @param  start_routine: The function to be called after the thread is launched.
 * @param  arg: The sole argument of the start_routine to be provided (Not implemented yet).
 * @retval int: 0 on sucess and nonzero error number on error.
 */
int pthread_create(pthread_t *thread, const pthread_attr_t *attr,
                   void *(*start_routine) (void *), void *arg);

/**
 * @brief  Return the thread ID of the calling thread
 * @param  None
 * @retval pthread_t: The hread ID.
 */
pthread_t pthread_self(void);

/**
 * @brief  Wait for a thread specified with the thread ID to terminate
 * @param  thread: The thread ID to be provided.
 * @param  retval: Not implemented yet.
 * @retval int: 0 on sucess and nonzero error number on error.
 */
int pthread_join(pthread_t thread, void **retval);

/**
 * @brief  Send a cancellation request to a thread specified with the thread ID
 * @param  thread: The thread ID to provide.
 * @retval int: 0 on sucess and nonzero error number on error.
 */
int pthread_cancel(pthread_t thread);

/**
 * @brief  Set the scheduling parameters of a thread specified with the thread ID
 * @param  thread: Thread ID to provide.
 * @param  policy: Not used.
 * @param  param: The scheduling parameter to set the thread.
 * @retval int: 0 on sucess and nonzero error number on error.
 */
int pthread_setschedparam(pthread_t thread, int policy, const struct sched_param *param);

/**
 * @brief  Get the scheduling parameters of a thread specified with the thread ID
 * @param  thread: The hread ID to provide.
 * @param  policy: Not used.
 * @param  param: For returning the scheduling parameter from the thread.
 * @retval int: 0 on sucess and nonzero error number on error.
 */
int pthread_getschedparam(pthread_t thread, int *policy, struct sched_param *param);

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
 * @param  retval: Not implemented yet.
 * @retval None
 */
void pthread_exit(void *retval);

/**
 * @brief  Initialize a mutex with specified attributes
 * @param  mutex: The mutex object to be initialized.
 * @param  attr: The attribute object for initializing the mutex.
 * @retval int: 0 on sucess and nonzero error number on error.
 */
int pthread_mutex_init(pthread_mutex_t *mutex, const pthread_mutex_attr_t *attr);

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
 * @brief  Initializes a conditional variable with specified attributes
 * @param  cond: The conditional variable object to be initialized.
 * @param  cond_attr: The attribute object for initializing the conditional variable.
 * @retval int: 0 on sucess and nonzero error number on error.
 */
int pthread_cond_init(pthread_cond_t *cond, pthread_condattr_t cond_attr);

/**
 * @brief  Restart one of the threads that are waiting on the condition variable
 * @param  cond: The conditional variable object to signal the state change.
 * @retval int: 0 on sucess and nonzero error number on error.
 */
int pthread_cond_signal(pthread_cond_t *cond);

/**
 * @brief  Atomically unlocks the mutex and waits for the condition variable to be signaled
 * @param  cond: The sonditional variable object for waiting the state change.
 * @retval int: 0 on sucess and nonzero error number on error.
 */
int pthread_cond_wait(pthread_cond_t *cond, pthread_mutex_t *mutex);

/**
 * @brief  To ensure that a piece of initialization code is executed at most once
 * @param  once_control: The object to track the execution state of the init_routine.
 * @param  init_routine: The function to be called for once.
 * @retval int: 0 on sucess and nonzero error number on error.
 */
int pthread_once(pthread_once_t *once_control, void (*init_routine)(void));

#endif
