#include <tenok.h>
#include <errno.h>
#include <pthread.h>

#include <arch/port.h>
#include <kernel/mutex.h>
#include <kernel/syscall.h>

int pthread_attr_init(pthread_attr_t *attr)
{
    attr->schedparam.sched_priority = 0;
    attr->stacksize = 512;
    attr->stackaddr = NULL;
    attr->detachstate = PTHREAD_CREATE_JOINABLE;
    return 0;
}

int pthread_attr_setschedparam(pthread_attr_t *attr, const struct sched_param *param)
{
    attr->schedparam = *param;
    return 0;
}

int pthread_attr_getschedparam(const pthread_attr_t *attr, struct sched_param *param)
{
    *param = attr->schedparam;
    return 0;
}

int pthread_attr_setstacksize(pthread_attr_t *attr, size_t stacksize)
{
    attr->stacksize = stacksize;
    return 0;
}

int pthread_attr_getstacksize(const pthread_attr_t *attr, size_t *stacksize)
{
    *stacksize = attr->stacksize;
    return 0;
}

int pthread_attr_setdetachstate(pthread_attr_t *attr, int detachstate)
{
    if(attr->detachstate != PTHREAD_CREATE_DETACHED &&
       attr->detachstate != PTHREAD_CREATE_JOINABLE) {
        return -EINVAL;
    }

    attr->detachstate = detachstate;
    return 0;
}

int pthread_attr_getdetachstate(const pthread_attr_t *attr, int *detachstate)
{
    *detachstate = attr->detachstate;
    return 0;
}

NACKED int pthread_create(pthread_t *thread, const pthread_attr_t *attr,
                          void *(*start_routine) (void *), void *arg)
{
    SYSCALL(PTHREAD_CREATE);
}

pthread_t pthread_self(void)
{
    return gettid();
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

NACKED int pthread_setschedparam(pthread_t thread, int policy,
                                 const struct sched_param *param)
{
    SYSCALL(PTHREAD_SETSCHEDPARAM);
}

NACKED int pthread_getschedparam(pthread_t thread, int *policy,
                                 struct sched_param *param)
{
    SYSCALL(PTHREAD_GETSCHEDPARAM);
}

int pthread_mutex_init(pthread_mutex_t *mutex, const pthread_mutex_attr_t *attr)
{
    mutex_init(mutex);
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

int pthread_cond_init(pthread_cond_t *cond, pthread_condattr_t cond_attr)
{
    list_init(&cond->task_wait_list);

    return 0;
}

NACKED int pthread_cond_signal(pthread_cond_t *cond)
{
    SYSCALL(PTHREAD_COND_SIGNAL);
}

NACKED int pthread_cond_wait(pthread_cond_t *cond, pthread_mutex_t *mutex)
{
    SYSCALL(PTHREAD_COND_WAIT);
}

NACKED int pthread_once(pthread_once_t *once_control, void (*init_routine)(void))
{
    SYSCALL(PTHREAD_ONCE);
}
