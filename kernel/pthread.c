#include <errno.h>
#include <pthread.h>
#include <sched.h>
#include <string.h>

#include <arch/port.h>
#include <common/list.h>
#include <kernel/errno.h>
#include <kernel/mutex.h>
#include <kernel/syscall.h>
#include <kernel/thread.h>

#include "kconfig.h"

/* A program declares its objects with the opaque types, and the calls below
 * write to the structures through them. The two sizes are written down in two
 * places and nothing but this says they still agree; when they stop agreeing,
 * a call writes past the end of what the program set aside
 */
_Static_assert(sizeof(pthread_mutexattr_t) == sizeof(struct mutex_attr),
               "pthread_mutexattr_t no longer holds a struct mutex_attr");
_Static_assert(sizeof(pthread_mutex_t) == sizeof(struct mutex),
               "pthread_mutex_t no longer holds a struct mutex");
_Static_assert(sizeof(pthread_attr_t) == sizeof(struct thread_attr),
               "pthread_attr_t no longer holds a struct thread_attr");
_Static_assert(sizeof(pthread_cond_t) == sizeof(struct cond),
               "pthread_cond_t no longer holds a struct cond");
_Static_assert(sizeof(pthread_once_t) == sizeof(struct thread_once),
               "pthread_once_t no longer holds a struct thread_once");

int pthread_attr_init(pthread_attr_t *attr)
{
    if (!attr)
        return EINVAL;

    struct thread_attr *_attr = (struct thread_attr *) attr;
    _attr->schedparam.sched_priority = 0;
    _attr->stacksize = STACK_SIZE_MIN;
    _attr->stackaddr = NULL;
    _attr->schedpolicy = SCHED_RR;
    _attr->inheritsched = PTHREAD_EXPLICIT_SCHED;
    _attr->detachstate = PTHREAD_CREATE_JOINABLE;
    return 0;
}

int pthread_attr_destroy(pthread_attr_t *attr)
{
    if (!attr)
        return EINVAL;

    struct thread_attr *_attr = (struct thread_attr *) attr;
    memset(_attr, 0, sizeof(struct thread_attr));

    return 0;
}

int pthread_attr_setschedparam(pthread_attr_t *attr,
                               const struct sched_param *param)
{
    if (!attr || !param)
        return EINVAL;

    struct thread_attr *_attr = (struct thread_attr *) attr;
    _attr->schedparam = *param;

    return 0;
}

int pthread_attr_getschedparam(const pthread_attr_t *attr,
                               struct sched_param *param)
{
    if (!attr || !param)
        return EINVAL;

    struct thread_attr *_attr = (struct thread_attr *) attr;
    *param = _attr->schedparam;

    return 0;
}

int pthread_attr_setschedpolicy(pthread_attr_t *attr, int policy)
{
    if (!attr)
        return EINVAL;

    struct thread_attr *_attr = (struct thread_attr *) attr;
    _attr->schedpolicy = policy;

    return 0;
}

int pthread_attr_getschedpolicy(const pthread_attr_t *attr, int *policy)
{
    if (!attr || !policy)
        return EINVAL;

    struct thread_attr *_attr = (struct thread_attr *) attr;
    *policy = _attr->schedpolicy;

    return 0;
}

int pthread_mutexattr_setprotocol(pthread_mutexattr_t *attr, int protocol)
{
    if (!attr)
        return EINVAL;

    struct mutex_attr *mtx_attr = (struct mutex_attr *) attr;
    mtx_attr->protocol = protocol;

    return 0;
}

int pthread_mutexattr_getprotocol(const pthread_mutexattr_t *attr,
                                  int *protocol)
{
    if (!attr || !protocol)
        return EINVAL;

    struct mutex_attr *mtx_attr = (struct mutex_attr *) attr;
    *protocol = mtx_attr->protocol;

    return 0;
}

int pthread_mutexattr_settype(pthread_mutexattr_t *attr, int type)
{
    if (!attr)
        return EINVAL;

    if (type != PTHREAD_MUTEX_NORMAL && type != PTHREAD_MUTEX_RECURSIVE)
        return EINVAL;

    ((struct mutex_attr *) attr)->type = type;

    return 0;
}

int pthread_mutexattr_gettype(const pthread_mutexattr_t *attr, int *type)
{
    if (!attr || !type)
        return EINVAL;

    *type = ((struct mutex_attr *) attr)->type;

    return 0;
}

int pthread_attr_setstacksize(pthread_attr_t *attr, size_t stacksize)
{
    if (!attr)
        return EINVAL;

    /* Say so here rather than let the thread run off the end of a stack that
     * was never going to be enough
     */
    if (stacksize < STACK_SIZE_MIN)
        return EINVAL;

    struct thread_attr *_attr = (struct thread_attr *) attr;
    _attr->stacksize = stacksize;

    return 0;
}

int pthread_attr_getstacksize(const pthread_attr_t *attr, size_t *stacksize)
{
    if (!attr || !stacksize)
        return EINVAL;

    struct thread_attr *_attr = (struct thread_attr *) attr;
    *stacksize = _attr->stacksize;

    return 0;
}

int pthread_attr_setdetachstate(pthread_attr_t *attr, int detachstate)
{
    if (!attr)
        return EINVAL;

    struct thread_attr *_attr = (struct thread_attr *) attr;

    if (detachstate != PTHREAD_CREATE_DETACHED &&
        detachstate != PTHREAD_CREATE_JOINABLE) {
        return EINVAL;
    }

    _attr->detachstate = detachstate;

    return 0;
}

int pthread_attr_getdetachstate(const pthread_attr_t *attr, int *detachstate)
{
    if (!attr || !detachstate)
        return EINVAL;

    struct thread_attr *_attr = (struct thread_attr *) attr;
    *detachstate = _attr->detachstate;

    return 0;
}

int pthread_attr_setinheritsched(pthread_attr_t *attr, int inheritsched)
{
    if (!attr)
        return EINVAL;

    if (inheritsched != PTHREAD_INHERIT_SCHED &&
        inheritsched != PTHREAD_EXPLICIT_SCHED)
        return EINVAL;

    ((struct thread_attr *) attr)->inheritsched = inheritsched;

    return 0;
}

int pthread_attr_getinheritsched(const pthread_attr_t *attr, int *inheritsched)
{
    if (!attr || !inheritsched)
        return EINVAL;

    *inheritsched = ((struct thread_attr *) attr)->inheritsched;

    return 0;
}

int pthread_attr_setstackaddr(pthread_attr_t *attr, void *stackaddr)
{
    if (!attr)
        return EINVAL;

    if (!stackaddr)
        return EINVAL;

    struct thread_attr *_attr = (struct thread_attr *) attr;
    _attr->stackaddr = stackaddr;

    return 0;
}

int pthread_attr_getstackaddr(const pthread_attr_t *attr, void **stackaddr)
{
    if (!attr || !stackaddr)
        return EINVAL;

    struct thread_attr *_attr = (struct thread_attr *) attr;
    *stackaddr = _attr->stackaddr;

    return 0;
}

/* A thread call answers with the number instead of leaving it in errno */
static inline int set_error(int retval)
{
    return (retval < 0) ? -retval : retval;
}

static NACKED int __pthread_create(pthread_t *thread,
                                   const pthread_attr_t *attr,
                                   void *(*start_routine)(void *),
                                   void *arg)
{
    SYSCALL(PTHREAD_CREATE);
}

int pthread_create(pthread_t *thread,
                   const pthread_attr_t *attr,
                   void *(*start_routine)(void *),
                   void *arg)
{
    return set_error(__pthread_create(thread, attr, start_routine, arg));
}

NACKED pthread_t pthread_self(void)
{
    SYSCALL(PTHREAD_SELF);
}

static NACKED int __pthread_join(pthread_t thread, void **retval)
{
    SYSCALL(PTHREAD_JOIN);
}

int pthread_join(pthread_t thread, void **retval)
{
    return set_error(__pthread_join(thread, retval));
}

static NACKED int __pthread_detach(pthread_t thread)
{
    SYSCALL(PTHREAD_DETACH);
}

int pthread_detach(pthread_t thread)
{
    return set_error(__pthread_detach(thread));
}

static NACKED int __pthread_cancel(pthread_t thread)
{
    SYSCALL(PTHREAD_CANCEL);
}

int pthread_cancel(pthread_t thread)
{
    return set_error(__pthread_cancel(thread));
}

int pthread_equal(pthread_t t1, pthread_t t2)
{
    return t1 == t2;
}

static NACKED int __pthread_setschedparam(pthread_t thread,
                                          int policy,
                                          const struct sched_param *param)
{
    SYSCALL(PTHREAD_SETSCHEDPARAM);
}

int pthread_setschedparam(pthread_t thread,
                          int policy,
                          const struct sched_param *param)
{
    return set_error(__pthread_setschedparam(thread, policy, param));
}

static NACKED int __pthread_getschedparam(pthread_t thread,
                                          int *policy,
                                          struct sched_param *param)
{
    SYSCALL(PTHREAD_GETSCHEDPARAM);
}

int pthread_getschedparam(pthread_t thread,
                          int *policy,
                          struct sched_param *param)
{
    return set_error(__pthread_getschedparam(thread, policy, param));
}

int pthread_mutexattr_init(pthread_mutexattr_t *attr)
{
    if (!attr)
        return EINVAL;

    struct mutex_attr *_attr = (struct mutex_attr *) attr;
    _attr->protocol = PTHREAD_PRIO_NONE;
    _attr->type = PTHREAD_MUTEX_DEFAULT;

    return 0;
}

int pthread_mutexattr_destroy(pthread_mutexattr_t *attr)
{
    if (!attr)
        return EINVAL;

    memset(attr, 0, sizeof(pthread_mutexattr_t));
    return 0;
}

int pthread_mutex_init(pthread_mutex_t *mutex, const pthread_mutexattr_t *attr)
{
    if (!mutex)
        return EINVAL;

    struct mutex *_mutex = (struct mutex *) mutex;
    __mutex_init(_mutex);

    if (attr) {
        struct mutex_attr *_attr = (struct mutex_attr *) attr;
        _mutex->protocol = _attr->protocol;
        _mutex->type = _attr->type;
    }

    return 0;
}

int pthread_mutex_destroy(pthread_mutex_t *mutex)
{
    if (!mutex)
        return EINVAL;

    memset(mutex, 0, sizeof(pthread_mutex_t));
    return 0;
}

static NACKED int __pthread_yield(void)
{
    SYSCALL(PTHREAD_YIELD);
}

int pthread_yield(void)
{
    return set_error(__pthread_yield());
}

static NACKED int __pthread_kill(pthread_t thread, int sig)
{
    SYSCALL(PTHREAD_KILL);
}

int pthread_kill(pthread_t thread, int sig)
{
    return set_error(__pthread_kill(thread, sig));
}

NACKED void pthread_exit(void *retval)
{
    SYSCALL(PTHREAD_EXIT);
}

static NACKED int __pthread_mutex_unlock(pthread_mutex_t *mutex)
{
    SYSCALL(PTHREAD_MUTEX_UNLOCK);
}

int pthread_mutex_unlock(pthread_mutex_t *mutex)
{
    return set_error(__pthread_mutex_unlock(mutex));
}

static NACKED int __pthread_mutex_lock(pthread_mutex_t *mutex)
{
    SYSCALL(PTHREAD_MUTEX_LOCK);
}

int pthread_mutex_lock(pthread_mutex_t *mutex)
{
    return set_error(__pthread_mutex_lock(mutex));
}

static NACKED int __pthread_mutex_trylock(pthread_mutex_t *mutex)
{
    SYSCALL(PTHREAD_MUTEX_TRYLOCK);
}

int pthread_mutex_trylock(pthread_mutex_t *mutex)
{
    return set_error(__pthread_mutex_trylock(mutex));
}

static NACKED int __pthread_mutex_timedlock(pthread_mutex_t *mutex,
                                            const struct timespec *abstime)
{
    SYSCALL(PTHREAD_MUTEX_TIMEDLOCK);
}

int pthread_mutex_timedlock(pthread_mutex_t *mutex,
                            const struct timespec *abstime)
{
    return set_error(__pthread_mutex_timedlock(mutex, abstime));
}
int pthread_condattr_init(pthread_condattr_t *attr)
{
    if (!attr)
        return EINVAL;

    /* No attribute is currently implemented */

    return 0;
}

int pthread_condattr_destroy(pthread_condattr_t *attr)
{
    if (!attr)
        return EINVAL;

    memset(attr, 0, sizeof(pthread_condattr_t));
    return 0;
}

int pthread_cond_init(pthread_cond_t *cond, const pthread_condattr_t *cond_attr)
{
    if (!cond)
        return EINVAL;

    INIT_LIST_HEAD(&((struct cond *) cond)->task_wait_list);
    return 0;
}

int pthread_cond_destroy(pthread_cond_t *cond)
{
    if (!cond)
        return EINVAL;

    memset(cond, 0, sizeof(pthread_cond_t));
    return 0;
}

static NACKED int __pthread_cond_signal(pthread_cond_t *cond)
{
    SYSCALL(PTHREAD_COND_SIGNAL);
}

int pthread_cond_signal(pthread_cond_t *cond)
{
    return set_error(__pthread_cond_signal(cond));
}

static NACKED int __pthread_cond_broadcast(pthread_cond_t *cond)
{
    SYSCALL(PTHREAD_COND_BROADCAST);
}

int pthread_cond_broadcast(pthread_cond_t *cond)
{
    return set_error(__pthread_cond_broadcast(cond));
}

static NACKED int __pthread_cond_wait(pthread_cond_t *cond,
                                      pthread_mutex_t *mutex)
{
    SYSCALL(PTHREAD_COND_WAIT);
}

int pthread_cond_wait(pthread_cond_t *cond, pthread_mutex_t *mutex)
{
    return set_error(__pthread_cond_wait(cond, mutex));
}

static NACKED int __pthread_cond_timedwait(pthread_cond_t *cond,
                                           pthread_mutex_t *mutex,
                                           const struct timespec *abstime)
{
    SYSCALL(PTHREAD_COND_TIMEDWAIT);
}

int pthread_cond_timedwait(pthread_cond_t *cond,
                           pthread_mutex_t *mutex,
                           const struct timespec *abstime)
{
    return set_error(__pthread_cond_timedwait(cond, mutex, abstime));
}

static NACKED int __pthread_once_begin(pthread_once_t *once_control)
{
    SYSCALL(PTHREAD_ONCE_BEGIN);
}

static NACKED int __pthread_once_end(pthread_once_t *once_control)
{
    SYSCALL(PTHREAD_ONCE_END);
}

/* The routine belongs to the caller and is called from here, where the caller
 * is, rather than from the kernel. The kernel only says which thread is to
 * call it, and hears back when it has been called
 */
int pthread_once(pthread_once_t *once_control, void (*init_routine)(void))
{
    if (!once_control || !init_routine)
        return EINVAL;

    if (__pthread_once_begin(once_control) == 0) {
        init_routine();
        __pthread_once_end(once_control);
    }

    return 0;
}

/* Tenok has no fork() for these to be run either side of, so there is never
 * anything to remember
 */
int pthread_atfork(void (*prepare)(void),
                   void (*parent)(void),
                   void (*child)(void))
{
    return ENOSYS;
}
