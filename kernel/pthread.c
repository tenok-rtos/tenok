#include <arch/port.h>
#include <kernel/syscall.h>

#include <tenok/pthread.h>

NACKED int pthread_mutex_init(pthread_mutex_t *mutex, const pthread_mutex_attr_t *attr)
{
    SYSCALL(PTHREAD_MUTEX_INIT);
}

NACKED int pthread_mutex_unlock(pthread_mutex_t *mutex)
{
    SYSCALL(PTHREAD_MUTEX_UNLOCK);
}

NACKED int pthread_mutex_lock(pthread_mutex_t *mutex)
{
    SYSCALL(PTHREAD_MUTEX_LOCK);
}

NACKED int pthread_cond_init(pthread_cond_t *cond, pthread_condattr_t cond_attr)
{
    SYSCALL(PTHREAD_COND_INIT);
}

NACKED int pthread_cond_signal(pthread_cond_t *cond)
{
    SYSCALL(PTHREAD_COND_SIGNAL);
}

NACKED int pthread_cond_wait(pthread_cond_t *cond, pthread_mutex_t *mutex)
{
    SYSCALL(PTHREAD_COND_WAIT);
}
