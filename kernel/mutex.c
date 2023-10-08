#include <stdbool.h>
#include <errno.h>

#include <kernel/errno.h>
#include <kernel/mutex.h>
#include <kernel/kernel.h>

void mutex_init(struct mutex *mtx)
{
    mtx->owner = NULL;
    list_init(&mtx->wait_list);
}

bool mutex_is_locked(struct mutex *mtx)
{
    return mtx->owner != NULL;
}

int mutex_lock(struct mutex *mtx)
{
    CURRENT_THREAD_INFO(curr_thread);

    /* check if the mutex is occupied */
    if(mtx->owner != NULL) {
        /* put the current thread into the mutex waiting list */
        prepare_to_wait(&mtx->wait_list, &curr_thread->list, THREAD_WAIT);

        return -ERESTARTSYS;
    } else {
        /* occupy the mutex by setting the owner */
        mtx->owner = curr_thread;

        return 0;
    }
}

int mutex_unlock(struct mutex *mtx)
{
    CURRENT_THREAD_INFO(curr_thread);

    /* only the owner thread can unlock the mutex */
    if(mtx->owner != curr_thread) {
        return -EPERM;
    }

    /* release the mutex */
    mtx->owner = NULL;

    /* check the mutex waiting list */
    if(!list_is_empty(&mtx->wait_list)) {
        /* wake up a thread from the mutex waiting list */
        wake_up(&mtx->wait_list);
    }

    return 0;
}
