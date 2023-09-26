#include <arch/port.h>
#include <kernel/syscall.h>

#include <tenok/pthread.h>

int pthread_mutex_init(pthread_mutex_t *mutex, const pthread_mutex_attr_t *attr)
{
    mutex->owner = NULL;
    mutex->lock = 0;
    list_init(&mutex->wait_list);

    return 0;
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
