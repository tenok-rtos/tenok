#include <stddef.h>
#include <stdint.h>
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

    /* block the current task if the request size is larger than the fifo can serve */
    if(size > pipe->count) {
        /* put the current task into the waiting list */
        prepare_to_wait(&pipe->task_wait_list, &curr_task->list, TASK_WAIT);

        /* turn on the syscall pending flag */
        curr_task->syscall_pending = true;

        return 0;
    } else {
        /* request can be fulfilled, turn off the syscall pending flag */
        curr_task->syscall_pending = false;
    }

    /* pop data from the pipe */
    int i;
    for(i = 0; i < size; i++) {
        ringbuf_get(pipe, &buf[i]);
    }

    return size;

}

ssize_t generic_pipe_write(pipe_t *pipe, const char *buf, size_t size, loff_t offset)
{
    struct list *wait_list = &pipe->task_wait_list;

    /* push data into the pipe */
    int i;
    for(i = 0; i < size; i++) {
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

    return size;
}
