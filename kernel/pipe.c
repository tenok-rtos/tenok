#include <stddef.h>
#include <stdint.h>
#include <errno.h>
#include "ringbuf.h"
#include "kconfig.h"
#include "kernel.h"
#include "pipe.h"

pipe_t *generic_pipe_create(size_t nmem, size_t size)
{
    return ringbuf_create(nmem, size);
}

ssize_t generic_pipe_read(pipe_t *pipe, char *buf, size_t size, loff_t offset)
{
    CURRENT_TASK_INFO(curr_task);

    ssize_t retval = 0;

    /* start of the critical section */
    spin_lock_irq(&pipe->lock);

    /* block the current task if the request size is larger than the fifo can serve */
    if(size > pipe->count) {
        retval = -EAGAIN;

        /* block mode */
        if(!(pipe->flags & O_NONBLOCK)) {
            /* put the current task into the waiting list */
            prepare_to_wait(&pipe->task_wait_list, &curr_task->list, TASK_WAIT);

            /* turn on the syscall pending flag */
            curr_task->syscall_pending = true;
        }
    } else {
        /* pop data from the pipe */
        for(int i = 0; i < size; i++) {
            ringbuf_get(pipe, &buf[i]);
        }

        retval = pipe->type_size * size; //total read bytes

        /* request can be fulfilled, turn off the syscall pending flag */
        curr_task->syscall_pending = false;
    }

    /* end of the critical section */
    spin_unlock_irq(&pipe->lock);

    return retval;
}

ssize_t generic_pipe_write(pipe_t *pipe, const char *buf, size_t size, loff_t offset)
{
    struct list *wait_list = &pipe->task_wait_list;

    /* start of the critical section */
    spin_lock_irq(&pipe->lock);

    /* push data into the pipe */
    for(int i = 0; i < size; i++) {
        ringbuf_put(pipe, &buf[i]);
    }

    /* resume a blocked task if the pipe has enough data to serve */
    if(!list_is_empty(wait_list)) {
        struct task_ctrl_blk *task = list_entry(wait_list->next, struct task_ctrl_blk, list);

        /* check if request size of the blocked task can be fulfilled */
        if(task->file_request.size <= pipe->count) {
            /* wake up the task from the waiting list */
            wake_up(wait_list);
        }
    }

    /* end of the critical section */
    spin_unlock_irq(&pipe->lock);

    return pipe->type_size * size; //total written bytes
}
