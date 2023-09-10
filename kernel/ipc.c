#include <stddef.h>
#include <stdint.h>
#include <errno.h>
#include "tenok/fcntl.h"
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

    struct list *w_wait_list = &pipe->w_wait_list;

    /* check the ring buffer has enough data to read or not */
    if(size > ringbuf_get_cnt(pipe)) {
        /* failed to read */
        retval = -EAGAIN;
    } else {
        size_t type_size = ringbuf_get_type_size(pipe);

        /* pop data from the pipe */
        for(int i = 0; i < size; i++) {
            ringbuf_get(pipe, buf + (type_size * i));
        }

        /* calculate total read bytes */
        retval = type_size * size;
    }

    /* resume a blocked writting task if the ring buffer has enough space to write */
    if(!list_is_empty(w_wait_list)) {
        /* acquire the task control block */
        struct task_ctrl_blk *task = TASK_ENTRY(w_wait_list->next);

        /* check if request size of the blocked writting task can be fulfilled */
        if(task->file_request.size <= ringbuf_get_free_space(pipe)) {
            /* yes, wake up the task from the write waiting list */
            wake_up(w_wait_list);
        }
    }

    /* end of the critical section */
    spin_unlock_irq(&pipe->lock);

    return retval;
}

ssize_t generic_pipe_write_isr(pipe_t *pipe, const char *buf, size_t size)
{
    ssize_t retval = 0;

    /* start of the critical section */
    spin_lock_irq(&pipe->lock);

    struct list *r_wait_list = &pipe->r_wait_list;

    /* check the ring buffer has enough space to write or not*/
    if(size > ringbuf_get_free_space(pipe)) {
        /* failed to write */
        retval = -EAGAIN;
    } else {
        size_t type_size = ringbuf_get_type_size(pipe);

        /* push data into the pipe */
        for(int i = 0; i < size; i++) {
            ringbuf_put(pipe, buf + (type_size * i));
        }

        /* calculate total written bytes */
        retval = type_size * size;
    }

    /* resume a blocked reading task if the ring buffer has enough data to serve */
    if(!list_is_empty(r_wait_list)) {
        /* acquire the task control block */
        struct task_ctrl_blk *task = TASK_ENTRY(r_wait_list->next);

        /* check if request size of the blocked reading task can be fulfilled */
        if(task->file_request.size <= ringbuf_get_cnt(pipe)) {
            /* yes, wake up the task from the waiting list */
            wake_up(r_wait_list);
        }
    }

    /* end of the critical section */
    spin_unlock_irq(&pipe->lock);

    return retval;
}

ssize_t generic_pipe_read(pipe_t *pipe, char *buf, size_t size)
{
    CURRENT_TASK_INFO(curr_task);

    ssize_t retval = 0;

    /* start of the critical section */
    spin_lock_irq(&pipe->lock);

    struct list *w_wait_list = &pipe->w_wait_list;

    /* block the current task if the request size is larger than the fifo can serve */
    if(size > ringbuf_get_cnt(pipe)) {
        /* failed to read */
        retval = -EAGAIN;

        /* block mode */
        if(!(pipe->flags & O_NONBLOCK)) {
            /* put the current task into the read waiting list */
            prepare_to_wait(&pipe->r_wait_list, &curr_task->list, TASK_WAIT);

            /* turn on the syscall pending flag */
            curr_task->syscall_pending = true;
        }
    } else {
        size_t type_size = ringbuf_get_type_size(pipe);

        /* pop data from the pipe */
        for(int i = 0; i < size; i++) {
            ringbuf_get(pipe, buf + (type_size * i));
        }

        /* calculate total read bytes */
        retval = type_size * size;

        /* request is fulfilled, turn off the syscall pending flag */
        curr_task->syscall_pending = false;
    }

    /* resume a blocked writting task if the pipe has enough space to write */
    if(!list_is_empty(w_wait_list)) {
        /* acquire the task control block */
        struct task_ctrl_blk *task = TASK_ENTRY(w_wait_list->next);

        /* check if request size of the blocked writting task can be fulfilled */
        if(task->file_request.size <= ringbuf_get_free_space(pipe)) {
            /* yes, wake up the task from the write waiting list */
            wake_up(w_wait_list);
        }
    }

    /* end of the critical section */
    spin_unlock_irq(&pipe->lock);

    return retval;
}

ssize_t generic_pipe_write(pipe_t *pipe, const char *buf, size_t size)
{
    CURRENT_TASK_INFO(curr_task);

    ssize_t retval = 0;

    /* start of the critical section */
    spin_lock_irq(&pipe->lock);

    struct list *r_wait_list = &pipe->r_wait_list;

    /* check the ring buffer has enough space to write or not */
    if(size > ringbuf_get_free_space(pipe)) {
        /* failed to write */
        retval = -EAGAIN;

        /* block mode */
        if(!(pipe->flags & O_NONBLOCK)) {
            /* put the current task into the read waiting list */
            prepare_to_wait(&pipe->r_wait_list, &curr_task->list, TASK_WAIT);

            /* turn on the syscall pending flag */
            curr_task->syscall_pending = true;
        }
    } else {
        size_t type_size = ringbuf_get_type_size(pipe);

        /* push data into the pipe */
        for(int i = 0; i < size; i++) {
            ringbuf_put(pipe, buf + (type_size * i));
        }

        /* calculate total written bytes */
        retval = type_size * size;

        /* request is fulfilled, turn off the syscall pending flag */
        curr_task->syscall_pending = false;
    }

    /* resume a blocked reading task if the pipe has enough data to read */
    if(!list_is_empty(r_wait_list)) {
        /* acquire the task control block */
        struct task_ctrl_blk *task = TASK_ENTRY(r_wait_list->next);

        /* check if request size of the blocked reading task can be fulfilled */
        if(task->file_request.size <= ringbuf_get_cnt(pipe)) {
            /* yes, wake up the task from the read waiting list */
            wake_up(r_wait_list);
        }
    }

    /* end of the critical section */
    spin_unlock_irq(&pipe->lock);

    return retval;
}

ssize_t fifo_read(struct file *filp, char *buf, size_t size, off_t offset);
ssize_t fifo_write(struct file *filp, const char *buf, size_t size, off_t offset);

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
    pipe->file.f_inode = file_inode;
    files[fd] = &pipe->file;

    return 0;
}

ssize_t fifo_read(struct file *filp, char *buf, size_t size, off_t offset)
{
    pipe_t *pipe = container_of(filp, struct ringbuf, file);
    pipe->flags = filp->f_flags;
    return generic_pipe_read(pipe, buf, size);
}

ssize_t fifo_write(struct file *filp, const char *buf, size_t size, off_t offset)
{
    pipe_t *pipe = container_of(filp, struct ringbuf, file);
    pipe->flags = filp->f_flags;
    return generic_pipe_write(pipe, buf, size);
}
