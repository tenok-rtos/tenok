#include <errno.h>
#include <semaphore.h>
#include <stdint.h>
#include <string.h>

#include <arch/port.h>
#include <common/list.h>
#include <kernel/errno.h>
#include <kernel/interrupt.h>
#include <kernel/kernel.h>
#include <kernel/sched.h>
#include <kernel/semaphore.h>
#include <kernel/syscall.h>
#include <kernel/thread.h>
#include <kernel/wait.h>

void sema_init(struct semaphore *sem, int val)
{
    sem->count = val;
    INIT_LIST_HEAD(&sem->wait_list);
}

int down(struct semaphore *sem)
{
    while (sem->count <= 0) {
        /* Failed to acquire the semaphore, enqueue the current thread into the
         * waiting list */
        preempt_disable();
        prepare_to_wait(&sem->wait_list, current_thread_info(), THREAD_WAIT);
        preempt_enable();

        schedule();
    }

    preempt_disable();
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
        return -ENOMEM;

    sema_init((struct semaphore *) sem, value);
    return 0;
}

int sem_destroy(sem_t *sem)
{
    if (!sem)
        return -ENOMEM;

    memset(sem, 0, sizeof(sem_t));
    return 0;
}

NACKED int sem_post(sem_t *sem)
{
    SYSCALL(SEM_POST);
}

NACKED int sem_trywait(sem_t *sem)
{
    SYSCALL(SEM_TRYWAIT);
}

NACKED int sem_wait(sem_t *sem)
{
    SYSCALL(SEM_WAIT);
}

NACKED int sem_getvalue(sem_t *sem, int *sval)
{
    SYSCALL(SEM_GETVALUE);
}
