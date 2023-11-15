#include <errno.h>
#include <pthread.h>
#include <string.h>
#include <tenok.h>

#include <arch/port.h>
#include <kernel/mutex.h>
#include <kernel/syscall.h>
#include <kernel/thread.h>

int pthread_attr_init(pthread_attr_t *attr)
{
    if (!attr)
        return -ENOMEM;

    struct thread_attr *_attr = (struct thread_attr *) attr;
    _attr->schedparam.sched_priority = 0;
    _attr->stacksize = 512;
    _attr->stackaddr = NULL;
    _attr->schedpolicy = SCHED_RR;
    _attr->detachstate = PTHREAD_CREATE_JOINABLE;
    return 0;
}

int pthread_attr_destroy(pthread_attr_t *attr)
{
    if (!attr)
        return -ENOMEM;

    struct thread_attr *_attr = (struct thread_attr *) attr;
    memset(_attr, 0, sizeof(struct thread_attr));

    return 0;
}

int pthread_attr_setschedparam(pthread_attr_t *attr,
                               const struct sched_param *param)
{
    if (!attr || !param)
        return -ENOMEM;

    struct thread_attr *_attr = (struct thread_attr *) attr;
    _attr->schedparam = *param;

    return 0;
}

int pthread_attr_getschedparam(const pthread_attr_t *attr,
                               struct sched_param *param)
{
    if (!attr || !param)
        return -ENOMEM;

    struct thread_attr *_attr = (struct thread_attr *) attr;
    *param = _attr->schedparam;

    return 0;
}

int pthread_attr_setschedpolicy(pthread_attr_t *attr, int policy)
{
    if (!attr)
        return -ENOMEM;

    struct thread_attr *_attr = (struct thread_attr *) attr;
    _attr->schedpolicy = policy;

    return 0;
}

int pthread_attr_getschedpolicy(const pthread_attr_t *attr, int *policy)
{
    if (!attr | !policy)
        return -ENOMEM;

    struct thread_attr *_attr = (struct thread_attr *) attr;
    *policy = _attr->schedpolicy;

    return 0;
}

int pthread_mutexattr_setprotocol(pthread_mutexattr_t *attr, int protocol)
{
    if (!attr)
        return -ENOMEM;

    struct mutex_attr *mtx_attr = (struct mutex_attr *) attr;
    mtx_attr->protocol = protocol;

    return 0;
}

int pthread_mutexattr_getprotocol(const pthread_mutexattr_t *attr,
                                  int *protocol)
{
    if (!attr)
        return -ENOMEM;

    struct mutex_attr *mtx_attr = (struct mutex_attr *) attr;
    *protocol = mtx_attr->protocol;

    return 0;
}

int pthread_attr_setstacksize(pthread_attr_t *attr, size_t stacksize)
{
    if (!attr)
        return -ENOMEM;

    struct thread_attr *_attr = (struct thread_attr *) attr;
    _attr->stacksize = stacksize;

    return 0;
}

int pthread_attr_getstacksize(const pthread_attr_t *attr, size_t *stacksize)
{
    if (!attr | !stacksize)
        return -ENOMEM;

    struct thread_attr *_attr = (struct thread_attr *) attr;
    *stacksize = _attr->stacksize;

    return 0;
}

int pthread_attr_setdetachstate(pthread_attr_t *attr, int detachstate)
{
    if (!attr)
        return -ENOMEM;

    struct thread_attr *_attr = (struct thread_attr *) attr;

    if (_attr->detachstate != PTHREAD_CREATE_DETACHED &&
        _attr->detachstate != PTHREAD_CREATE_JOINABLE) {
        return -EINVAL;
    }

    _attr->detachstate = detachstate;

    return 0;
}

int pthread_attr_getdetachstate(const pthread_attr_t *attr, int *detachstate)
{
    if (!attr | !detachstate)
        return -ENOMEM;

    struct thread_attr *_attr = (struct thread_attr *) attr;
    *detachstate = _attr->detachstate;

    return 0;
}

int pthread_attr_setstackaddr(pthread_attr_t *attr, void *stackaddr)
{
    if (!attr)
        return -ENOMEM;

    if (!stackaddr)
        return -EINVAL;

    struct thread_attr *_attr = (struct thread_attr *) attr;
    _attr->stackaddr = stackaddr;

    return 0;
}

int pthread_attr_getstackaddr(const pthread_attr_t *attr, void **stackaddr)
{
    if (!attr | !stackaddr)
        return -ENOMEM;

    struct thread_attr *_attr = (struct thread_attr *) attr;
    *stackaddr = _attr->stackaddr;

    return 0;
}

NACKED int pthread_create(pthread_t *thread,
                          const pthread_attr_t *attr,
                          void *(*start_routine)(void *),
                          void *arg)
{
    SYSCALL(PTHREAD_CREATE);
}

NACKED pthread_t pthread_self(void)
{
    SYSCALL(PTHREAD_SELF);
}

NACKED int pthread_join(pthread_t thread, void **retval)
{
    SYSCALL(PTHREAD_JOIN);
}

NACKED int pthread_detach(pthread_t thread)
{
    SYSCALL(PTHREAD_DETACH);
}

NACKED int pthread_cancel(pthread_t thread)
{
    SYSCALL(PTHREAD_CANCEL);
}

int pthread_equal(pthread_t t1, pthread_t t2)
{
    return t1 == t2;
}

NACKED int pthread_setschedparam(pthread_t thread,
                                 int policy,
                                 const struct sched_param *param)
{
    SYSCALL(PTHREAD_SETSCHEDPARAM);
}

NACKED int pthread_getschedparam(pthread_t thread,
                                 int *policy,
                                 struct sched_param *param)
{
    SYSCALL(PTHREAD_GETSCHEDPARAM);
}

int pthread_mutexattr_init(pthread_mutexattr_t *attr)
{
    if (!attr)
        return -ENOMEM;

    struct mutex_attr *_attr = (struct mutex_attr *) attr;
    _attr->protocol = PTHREAD_PRIO_NONE;

    return 0;
}

int pthread_mutexattr_destroy(pthread_mutexattr_t *attr)
{
    if (!attr)
        return -ENOMEM;

    memset(attr, 0, sizeof(pthread_mutexattr_t));
    return 0;
}

int pthread_mutex_init(pthread_mutex_t *mutex, const pthread_mutexattr_t *attr)
{
    if (!mutex)
        return -ENOMEM;

    struct mutex *_mutex = (struct mutex *) mutex;
    mutex_init(_mutex);

    if (attr) {
        struct mutex_attr *_attr = (struct mutex_attr *) attr;
        _mutex->protocol = _attr->protocol;
    }

    return 0;
}

int pthread_mutex_destroy(pthread_mutex_t *mutex)
{
    if (!mutex)
        return -ENOMEM;

    memset(mutex, 0, sizeof(pthread_mutex_t));
    return 0;
}

NACKED int pthread_yield(void)
{
    SYSCALL(PTHREAD_YIELD);
}

NACKED int pthread_kill(pthread_t thread, int sig)
{
    SYSCALL(PTHREAD_KILL);
}

NACKED void pthread_exit(void *retval)
{
    SYSCALL(PTHREAD_EXIT);
}

NACKED int pthread_mutex_unlock(pthread_mutex_t *mutex)
{
    SYSCALL(PTHREAD_MUTEX_UNLOCK);
}

NACKED int pthread_mutex_lock(pthread_mutex_t *mutex)
{
    SYSCALL(PTHREAD_MUTEX_LOCK);
}

NACKED int pthread_mutex_trylock(pthread_mutex_t *mutex)
{
    SYSCALL(PTHREAD_MUTEX_TRYLOCK);
}

int pthread_condattr_init(pthread_condattr_t *attr)
{
    if (!attr)
        return -ENOMEM;

    // no attribute is currently implemented

    return 0;
}

int pthread_condattr_destroy(pthread_condattr_t *attr)
{
    if (!attr)
        return -ENOMEM;

    memset(attr, 0, sizeof(pthread_condattr_t));
    return 0;
}

int pthread_cond_init(pthread_cond_t *cond, pthread_condattr_t cond_attr)
{
    if (!cond)
        return -ENOMEM;

    INIT_LIST_HEAD(&((struct cond *) cond)->task_wait_list);
    return 0;
}

int pthread_cond_destroy(pthread_cond_t *cond)
{
    if (!cond)
        return -ENOMEM;

    memset(cond, 0, sizeof(pthread_cond_t));
    return 0;
}

NACKED int pthread_cond_signal(pthread_cond_t *cond)
{
    SYSCALL(PTHREAD_COND_SIGNAL);
}

NACKED int pthread_cond_broadcast(pthread_cond_t *cond)
{
    SYSCALL(PTHREAD_COND_BROADCAST);
}

NACKED int pthread_cond_wait(pthread_cond_t *cond, pthread_mutex_t *mutex)
{
    SYSCALL(PTHREAD_COND_WAIT);
}

NACKED int pthread_once(pthread_once_t *once_control,
                        void (*init_routine)(void))
{
    SYSCALL(PTHREAD_ONCE);
}
