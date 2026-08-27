#include <errno.h>
#include <pthread.h>
#include <stdbool.h>
#include <stddef.h>
#include <string.h>

#include <common/list.h>
#include <kernel/errno.h>
#include <kernel/kernel.h>
#include <kernel/mutex.h>
#include <kernel/preempt.h>
#include <kernel/sched.h>
#include <kernel/thread.h>
#include <kernel/wait.h>

void __mutex_init(struct mutex *mtx)
{
    memset(mtx, 0, sizeof(*mtx));
    INIT_LIST_HEAD(&mtx->wait_list);
}

void mutex_init(struct mutex *mtx)
{
    __mutex_init(mtx);
    mtx->protocol = PTHREAD_PRIO_INHERIT;
}

/* A mutex written down with PTHREAD_MUTEX_INITIALIZER is all zeros, and a list
 * of zeros is not yet a list. The first thread to reach for it makes one, the
 * same way pthread_once() does
 */
static void mutex_check_init(struct mutex *mtx)
{
    if (!mtx->wait_list.next || !mtx->wait_list.prev)
        INIT_LIST_HEAD(&mtx->wait_list);
}

bool mutex_is_locked(struct mutex *mtx)
{
    preempt_disable();
    bool retval = mtx->owner != NULL;
    preempt_enable();

    return retval;
}

int mutex_trylock(struct mutex *mtx)
{
    preempt_disable();

    mutex_check_init(mtx);

    int retval;

    CURRENT_THREAD_INFO(curr_thread);

    /* Check if the mutex is occupied */
    if (mtx->owner == curr_thread && mtx->type == PTHREAD_MUTEX_RECURSIVE) {
        /* A recursive mutex is handed straight back to the thread already
         * holding it, and is only let go once it has been let go as many times
         * as it was taken
         */
        mtx->count++;

        retval = 0;
    } else if (mtx->owner != NULL) {
        retval = -EBUSY;
    } else {
        /* Occupy the mutex by setting the owner */
        mtx->owner = curr_thread;

        retval = 0;
    }

    preempt_enable();

    return retval;
}

int mutex_lock(struct mutex *mtx)
{
    int retval;
    CURRENT_THREAD_INFO(curr_thread);

    while (1) {
        retval = mutex_trylock(mtx);

        if (retval == -EBUSY) {
            thread_inherit_priority(mtx);
            prepare_to_wait(&mtx->wait_list, curr_thread, THREAD_WAIT);
        } else {
            break;
        }

        schedule();
    }

    /* Reset priority inheritance */
    if (retval == 0)
        thread_reset_inherited_priority(mtx);

    return retval;
}

int mutex_unlock(struct mutex *mtx)
{
    preempt_disable();

    mutex_check_init(mtx);

    int retval;

    CURRENT_THREAD_INFO(curr_thread);

    /* Only the owner thread can unlock the mutex */
    if (mtx->owner != curr_thread) {
        retval = -EPERM;
        goto leave;
    }

    /* A thread that took a recursive mutex more than once keeps it until it
     * has let go of it as many times
     */
    if (mtx->count > 0) {
        mtx->count--;
        retval = 0;
        goto leave;
    }

    /* Release the mutex */
    mtx->owner = NULL;

    /* Reset priority inheritance */
    thread_reset_inherited_priority(mtx);

    /* Wake up the highest-priority thread from the waiting list */
    wake_up(&mtx->wait_list);

    /* Return success */
    retval = 0;

leave:
    preempt_enable();
    return retval;
}
