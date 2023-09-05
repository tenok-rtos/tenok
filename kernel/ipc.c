#include <stddef.h>
#include <stdint.h>
#include <errno.h>
#include "fs.h"
#include "fifo.h"
#include "ringbuf.h"
#include "kconfig.h"
#include "kernel.h"
#include "pipe.h"

pipe_t *generic_pipe_create(size_t nmem, size_t size)
{
    return ringbuf_create(nmem, size);
}

ssize_t generic_pipe_read_isr(pipe_t *pipe, char *buf, size_t size)
{
    ssize_t retval = 0;

    /* start of the critical section */
    spin_lock_irq(&pipe->lock);

    if(size > pipe->count) {
        retval = -EAGAIN;
    } else {
        /* pop data from the pipe */
        for(int i = 0; i < size; i++) {
            ringbuf_get(pipe, &buf[i]);
        }

        retval = pipe->type_size * size; //total read bytes
    }

    /* end of the critical section */
    spin_unlock_irq(&pipe->lock);

    return retval;
}

ssize_t generic_pipe_write_isr(pipe_t *pipe, const char *buf, size_t size)
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


ssize_t generic_pipe_read(pipe_t *pipe, char *buf, size_t size)
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

ssize_t generic_pipe_write(pipe_t *pipe, const char *buf, size_t size)
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

ssize_t fifo_read(struct file *filp, char *buf, size_t size, loff_t offset);
ssize_t fifo_write(struct file *filp, const char *buf, size_t size, loff_t offset);

static struct file_operations fifo_ops = {
    .read = fifo_read,
    .write = fifo_write
};

int fifo_init(int fd, struct file **files, struct inode *file_inode)
{
    /* initialize the pipe */
    pipe_t *pipe = generic_pipe_create(sizeof(uint8_t), PIPE_DEPTH);

    /* register fifo to the file table */
    pipe->file.f_op = &fifo_ops;
    pipe->file.file_inode = file_inode;
    files[fd] = &pipe->file;

    return 0;
}

ssize_t fifo_read(struct file *filp, char *buf, size_t size, loff_t offset)
{
    pipe_t *pipe = container_of(filp, struct ringbuf, file);
    return generic_pipe_read(pipe, buf, size);
}

ssize_t fifo_write(struct file *filp, const char *buf, size_t size, loff_t offset)
{
    pipe_t *pipe = container_of(filp, struct ringbuf, file);
    return generic_pipe_write(pipe, buf, size);
}
