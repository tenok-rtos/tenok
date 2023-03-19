#include <stddef.h>
#include <errno.h>
#include "kernel.h"
#include "list.h"
#include "semaphore.h"
#include "syscall.h"

extern struct task_ctrl_blk *running_task;

int sem_init(sem_t *sem, int pshared, unsigned int value)
{
    sem->count = value;
    sem->lock = 0;
    list_init(&sem->wait_list);

    return 0;
}

/* sem_post() can be called by user task or interrupt handler functions */
int sem_post(sem_t *sem)
{
    /* start of the critical section */
    spin_lock_irq(&sem->lock);

    int retval;

    /* prevent integer overflow */
    if(sem->count >= (INT32_MAX - 1)) {
        retval = EAGAIN; //ask the user to try again
    } else {
        /* increase the semaphore */
        sem->count++;

        /* wake up a task from the waiting list */
        if(sem->count > 0 && !list_is_empty(&sem->wait_list)) {
            wake_up(&sem->wait_list);
        }
    }

    /* end of the critical section */
    spin_unlock_irq(&sem->lock);

    return 0;
}

/* sem_trywait() can be called in user space or kernel space */
int sem_trywait(sem_t *sem)
{
    /* start of the critical section */
    spin_lock_irq(&sem->lock);

    int retval;

    if(sem->count <= 0) {
        /* failed to obtain the semaphore, put the current task into the waiting list */
        prepare_to_wait(&sem->wait_list, &running_task->list, TASK_WAIT);
        retval = EAGAIN; //ask the user to try again
    } else {
        /* successfully obtained the semaphore */
        sem->count--;
        retval = 0;
    }

    /* end of the critical section */
    spin_unlock_irq(&sem->lock);

    return retval;
}

/* sem_wait() can only be called by user tasks */
int sem_wait(sem_t *sem)
{
    spin_lock_irq(&sem->lock);

    while(1) {
        if(sem->count <= 0) {
            /* failed to obtain the semaphore, put the current task into the waiting list */
            prepare_to_wait(&sem->wait_list, &running_task->list, TASK_WAIT);

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

int sem_getvalue(sem_t *sem, int *sval)
{
    *sval = sem->count;
    return 0;
}
