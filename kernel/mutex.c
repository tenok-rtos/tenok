#include <errno.h>
#include <stdbool.h>
#include <stddef.h>

#include <common/list.h>
#include <kernel/errno.h>
#include <kernel/kernel.h>
#include <kernel/mutex.h>
#include <kernel/thread.h>
#include <kernel/wait.h>

void mutex_init(struct mutex *mtx)
{
    mtx->owner = NULL;
    INIT_LIST_HEAD(&mtx->wait_list);
}

bool mutex_is_locked(struct mutex *mtx)
{
    return mtx->owner != NULL;
}

int mutex_lock(struct mutex *mtx)
{
    CURRENT_THREAD_INFO(curr_thread);

    /* Check if the mutex is occupied */
    if (mtx->owner != NULL) {
        /* Enqueue current thread into the waiting list */
        prepare_to_wait(&mtx->wait_list, curr_thread, THREAD_WAIT);

        return -ERESTARTSYS;
    } else {
        /* Occupy the mutex by setting the owner */
        mtx->owner = curr_thread;

        return 0;
    }
}

int mutex_unlock(struct mutex *mtx)
{
    CURRENT_THREAD_INFO(curr_thread);

    /* Only the owner thread can unlock the mutex */
    if (mtx->owner != curr_thread)
        return -EPERM;

    /* Release the mutex */
    mtx->owner = NULL;

    /* Wake up the highest-priority thread from the waiting list */
    wake_up(&mtx->wait_list);

    return 0;
}
