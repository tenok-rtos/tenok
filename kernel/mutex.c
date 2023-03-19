#include <stddef.h>
#include "kernel.h"
#include "list.h"
#include "mutex.h"
#include "syscall.h"

extern struct task_ctrl_blk *running_task;

int pthread_mutex_init(_pthread_mutex_t *mutex, const pthread_mutex_attr_t *attr)
{
    mutex->owner = NULL;
    mutex->lock = 0;
    list_init(&mutex->wait_list);

    return 0;
}

int pthread_mutex_unlock(_pthread_mutex_t *mutex)
{
    /* start of the critical section */
    spin_lock_irq(&mutex->lock);

    int retval = 0;

    /* check if mutex is already occupied */
    if(mutex->owner == running_task) {
        /* release the mutex */
        mutex->owner = NULL;

        /* check the mutex waiting list */
        if(!list_is_empty(&mutex->wait_list)) {
            /* wake up a task from the mutex waiting list */
            wake_up(&mutex->wait_list);
        }
    } else {
        retval = EPERM;
    }

    /* end of the critical section */
    spin_unlock_irq(&mutex->lock);

    return retval;
}

int pthread_mutex_lock(_pthread_mutex_t *mutex)
{
    while(1) {
        /* start of the critical section */
        spin_lock_irq(&mutex->lock);

        /* check if mutex is already occupied */
        if(mutex->owner != NULL) {
            /* put the current task into the mutex waiting list */
            prepare_to_wait(&mutex->wait_list, &running_task->list, TASK_WAIT);

            /* end of the critical section */
            spin_unlock_irq(&mutex->lock);

            /* sleep */
            sched_yield();
        } else {
            /* occupy the mutex by setting the owner */
            mutex->owner = running_task;

            /* end of the critical section */
            spin_unlock_irq(&mutex->lock);

            break;
        }
    }

    return 0;
}
