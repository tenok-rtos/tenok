#include <stddef.h>
#include <stdint.h>
#include <errno.h>

#include <fs/fs.h>
#include <kernel/ipc.h>
#include <kernel/wait.h>
#include <kernel/poll.h>
#include <kernel/kfifo.h>
#include <kernel/kernel.h>

#include <tenok/poll.h>
#include <tenok/fcntl.h>

#include "kconfig.h"

pipe_t *pipe_create_generic(size_t nmem, size_t size)
{
    return kfifo_alloc(nmem, size);
}

ssize_t pipe_read_generic(pipe_t *pipe, char *buf, size_t size)
{
    CURRENT_TASK_INFO(curr_task);

    ssize_t retval = 0;

    struct list *w_wait_list = &pipe->w_wait_list;

    size_t fifo_len = kfifo_len(pipe);

    /* block the current task if the request size is larger than the fifo can serve */
    if(size > fifo_len) {
        /* block mode */
        if(!(pipe->flags & O_NONBLOCK)) {
            /* put the current task into the read waiting list */
            prepare_to_wait(&pipe->r_wait_list, &curr_task->list, TASK_WAIT);

            /* turn on the syscall pending flag */
            set_syscall_pending(curr_task);
        } else {
            /* non-block mode */
            if(fifo_len > 0) {
                kfifo_out(pipe, buf, fifo_len);
                retval = fifo_len;
            } else {
                retval = -EAGAIN;
            }
        }
    } else {
        /* pop data from the pipe */
        kfifo_out(pipe, buf, size);

        /* calculate total read bytes */
        size_t type_size = kfifo_esize(pipe);
        retval = type_size * size;

        /* request is fulfilled, turn off the syscall pending flag */
        reset_syscall_pending(curr_task);
    }

    /* resume a blocked writting task if the pipe has enough space to write */
    if(!list_is_empty(w_wait_list)) {
        /* acquire the task control block */
        struct task_ctrl_blk *task = TASK_ENTRY(w_wait_list->next);

        /* check if request size of the blocked writting task can be fulfilled */
        if(task->file_request.size <= kfifo_avail(pipe)) {
            /* yes, wake up the task from the write waiting list */
            wake_up(w_wait_list);
        }
    }

    return retval;
}

ssize_t pipe_write_generic(pipe_t *pipe, const char *buf, size_t size)
{
    CURRENT_TASK_INFO(curr_task);

    ssize_t retval = 0;

    struct list *r_wait_list = &pipe->r_wait_list;

    size_t fifo_avail = kfifo_avail(pipe);

    /* check the ring buffer has enough space to write or not */
    if(size > fifo_avail) {
        /* failed to write */
        retval = -EAGAIN;

        /* block mode */
        if(!(pipe->flags & O_NONBLOCK)) {
            /* put the current task into the read waiting list */
            prepare_to_wait(&pipe->r_wait_list, &curr_task->list, TASK_WAIT);

            /* turn on the syscall pending flag */
            set_syscall_pending(curr_task);
        } else {
            /* non-block mode */
            if(fifo_avail > 0) {
                kfifo_in(pipe, buf, fifo_avail);
                retval = fifo_avail;
            } else {
                retval = -EAGAIN;
            }
        }
    } else {
        /* push data into the pipe */
        kfifo_in(pipe, buf, size);

        /* calculate total written bytes */
        size_t type_size = kfifo_esize(pipe);
        retval = type_size * size;

        /* request is fulfilled, turn off the syscall pending flag */
        reset_syscall_pending(curr_task);
    }

    /* resume a blocked reading task if the pipe has enough data to read */
    if(!list_is_empty(r_wait_list)) {
        /* acquire the task control block */
        struct task_ctrl_blk *task = TASK_ENTRY(r_wait_list->next);

        /* check if request size of the blocked reading task can be fulfilled */
        if(task->file_request.size <= kfifo_len(pipe)) {
            /* yes, wake up the task from the read waiting list */
            wake_up(r_wait_list);
        }
    }

    return retval;
}

ssize_t fifo_read(struct file *filp, char *buf, size_t size, off_t offset);
ssize_t fifo_write(struct file *filp, const char *buf, size_t size, off_t offset);

static struct file_operations fifo_ops = {
    .read = fifo_read,
    .write = fifo_write,
};

int fifo_init(int fd, struct file **files, struct inode *file_inode)
{
    /* initialize the pipe */
    pipe_t *pipe = pipe_create_generic(sizeof(uint8_t), PIPE_DEPTH);

    /* register fifo to the file table */
    pipe->file.f_op = &fifo_ops;
    pipe->file.f_inode = file_inode;
    files[fd] = &pipe->file;

    return 0;
}

ssize_t fifo_read(struct file *filp, char *buf, size_t size, off_t offset)
{
    pipe_t *pipe = container_of(filp, struct kfifo, file);
    pipe->flags = filp->f_flags;

    ssize_t retval = pipe_read_generic(pipe, buf, size);

    /* update file event */
    if(kfifo_len(pipe) > 0) {
        filp->f_events |= POLLOUT;
        poll_notify(filp);
    } else {
        filp->f_events &= ~POLLOUT;
    }

    return retval;
}

ssize_t fifo_write(struct file *filp, const char *buf, size_t size, off_t offset)
{
    pipe_t *pipe = container_of(filp, struct kfifo, file);
    pipe->flags = filp->f_flags;

    ssize_t retval = pipe_write_generic(pipe, buf, size);

    /* update file event */
    if(kfifo_avail(pipe) > 0) {
        filp->f_events |= POLLIN;
        poll_notify(filp);
    } else {
        filp->f_events &= ~POLLIN;
    }

    return retval;
}
