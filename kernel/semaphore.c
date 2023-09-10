#include <stddef.h>
#include <errno.h>
#include "tenok/sched.h"
#include "kernel.h"
#include "list.h"
#include "semaphore.h"

int sema_init(sem_t *sem, unsigned int value)
{
    sem->count = value;
    sem->lock = 0;
    list_init(&sem->wait_list);

    return 0;
}

int up(struct semaphore *sem)
{
    /* start of the critical section */
    spin_lock_irq(&sem->lock);

    int retval;

    /* prevent integer overflow */
    if(sem->count >= (INT32_MAX - 1)) {
        /* return on error */
        retval = -EAGAIN;
    } else {
        /* increase the semaphore */
        sem->count++;

        /* wake up a task from the waiting list */
        if(sem->count > 0 && !list_is_empty(&sem->wait_list)) {
            wake_up(&sem->wait_list);
        }

        /* return on success */
        retval = 0;
    }

    /* end of the critical section */
    spin_unlock_irq(&sem->lock);

    return retval;
}

int down_trylock(struct semaphore *sem)
{
    CURRENT_TASK_INFO(curr_task);

    /* start of the critical section */
    spin_lock_irq(&sem->lock);

    int retval;

    if(sem->count <= 0) {
        /* failed to obtain the semaphore, put the current task into the waiting list */
        prepare_to_wait(&sem->wait_list, &curr_task->list, TASK_WAIT);

        /* return on error */
        retval = -EAGAIN;
    } else {
        /* successfully obtained the semaphore */
        sem->count--;

        /* return on success */
        retval = 0;
    }

    /* end of the critical section */
    spin_unlock_irq(&sem->lock);

    return retval;
}

int down(struct semaphore *sem)
{
    CURRENT_TASK_INFO(curr_task);

    spin_lock_irq(&sem->lock);

    while(1) {
        if(sem->count <= 0) {
            /* failed to obtain the semaphore, put the current task into the waiting list */
            prepare_to_wait(&sem->wait_list, &curr_task->list, TASK_WAIT);

            /* end of the critical section */
            spin_unlock_irq(&sem->lock);

            /* start sleeping */
            sched_yield();
        } else {
            /* successfully obtained the semaphore */
            sem->count--;

            /* end of the critical section */
            spin_unlock_irq(&sem->lock);

            return 0;
        }
    }
}
