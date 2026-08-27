#include <errno.h>
#include <pthread.h>
#include <semaphore.h>
#include <stdint.h>
#include <string.h>

#include <arch/port.h>
#include <common/list.h>
#include <kernel/errno.h>
#include <kernel/kernel.h>
#include <kernel/preempt.h>
#include <kernel/sched.h>
#include <kernel/semaphore.h>
#include <kernel/syscall.h>
#include <kernel/thread.h>
#include <kernel/wait.h>

/* See the note beside the same check in kernel/pthread.c */
_Static_assert(sizeof(sem_t) == sizeof(struct semaphore),
               "sem_t no longer holds a struct semaphore");

void sema_init(struct semaphore *sem, int val)
{
    sem->count = val;
    INIT_LIST_HEAD(&sem->wait_list);
}

int down(struct semaphore *sem)
{
    preempt_disable();

    while (sem->count <= 0) {
        /* Failed to acquire the semaphore, enqueue the current thread into the
         * waiting list */
        prepare_to_wait(&sem->wait_list, current_thread_info(), THREAD_WAIT);

        schedule();

        /* Woken to be stopped rather than because the count moved. Waiting
         * again would put the thread back where it can never be reached
         */
        if (thread_cancel_requested()) {
            preempt_enable();
            return -ECANCELED;
        }
    }

    /* Acquired the semaphore successfully */
    sem->count--;

    preempt_enable();

    return 0;
}

int down_trylock(struct semaphore *sem)
{
    preempt_disable();

    int retval;

    if (sem->count <= 0) {
        retval = -EAGAIN;
    } else {
        /* Acquired the semaphore successfully */
        sem->count--;

        retval = 0;
    }

    preempt_enable();

    return retval;
}

int up(struct semaphore *sem)
{
    preempt_disable();

    int retval;

    /* Prevent the integer overflow */
    if (sem->count >= (INT32_MAX - 1)) {
        retval = -EOVERFLOW;
    } else {
        /* Increase the semaphore counter */
        sem->count++;

        /* Wake up the highest-priority thread from the waiting list */
        if (sem->count > 0 && !list_empty(&sem->wait_list)) {
            wake_up(&sem->wait_list);
        }

        retval = 0;
    }

    preempt_enable();

    return retval;
}

int sem_init(sem_t *sem, int pshared, unsigned int value)
{
    if (!sem)
        return -EINVAL;

    sema_init((struct semaphore *) sem, value);
    return 0;
}

int sem_destroy(sem_t *sem)
{
    if (!sem)
        return -EINVAL;

    memset(sem, 0, sizeof(sem_t));
    return 0;
}

static NACKED int __sem_post(sem_t *sem)
{
    SYSCALL(SEM_POST);
}

int sem_post(sem_t *sem)
{
    return set_errno(__sem_post(sem));
}

static NACKED int __sem_trywait(sem_t *sem)
{
    SYSCALL(SEM_TRYWAIT);
}

int sem_trywait(sem_t *sem)
{
    return set_errno(__sem_trywait(sem));
}

static NACKED int __sem_wait(sem_t *sem)
{
    SYSCALL(SEM_WAIT);
}

/* POSIX names this one of the places a thread that was asked to stop does */
int sem_wait(sem_t *sem)
{
    int retval = set_errno(__sem_wait(sem));

    pthread_testcancel();

    return retval;
}

static NACKED int __sem_timedwait(sem_t *sem, const struct timespec *abstime)
{
    SYSCALL(SEM_TIMEDWAIT);
}

int sem_timedwait(sem_t *sem, const struct timespec *abstime)
{
    int retval = set_errno(__sem_timedwait(sem, abstime));

    pthread_testcancel();

    return retval;
}

static NACKED int __sem_getvalue(sem_t *sem, int *sval)
{
    SYSCALL(SEM_GETVALUE);
}

int sem_getvalue(sem_t *sem, int *sval)
{
    return set_errno(__sem_getvalue(sem, sval));
}
