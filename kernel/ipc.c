#include <stddef.h>
#include <stdint.h>
#include <errno.h>
#include <poll.h>
#include <fcntl.h>

#include <fs/fs.h>
#include <kernel/ipc.h>
#include <kernel/wait.h>
#include <kernel/poll.h>
#include <kernel/errno.h>
#include <kernel/kfifo.h>
#include <kernel/kernel.h>

#include "kconfig.h"

pipe_t *pipe_create_generic(size_t nmem, size_t size)
{
    return kfifo_alloc(nmem, size);
}

ssize_t pipe_read_generic(pipe_t *pipe, char *buf, size_t size)
{
    CURRENT_THREAD_INFO(curr_thread);

    ssize_t retval = 0;
    size_t fifo_len = kfifo_len(pipe);
    struct list *w_wait_list = &pipe->w_wait_list;

    /* check if the request size is larger than the fifo can serve */
    if(size > fifo_len) {
        if(pipe->flags & O_NONBLOCK) { /* non-block mode */
            if(fifo_len > 0) {
                kfifo_out(pipe, buf, fifo_len);
                return fifo_len;
            } else {
                return -EAGAIN;
            }
        } else { /* block mode */
            /* put the current thread into the read waiting list */
            prepare_to_wait(&pipe->r_wait_list, &curr_thread->list, THREAD_WAIT);
            curr_thread->file_request.size = size;

            return -ERESTARTSYS;
        }
    }

    /* pop data from the pipe */
    kfifo_out(pipe, buf, size);

    /* calculate total read bytes */
    size_t type_size = kfifo_esize(pipe);
    retval = type_size * size;

    /* resume a blocked writting thread if the pipe has enough space to write */
    if(!list_is_empty(w_wait_list)) {
        /* acquire the thread info */
        struct thread_info *thread =
            list_entry(w_wait_list->next, struct thread_info, list);

        /* check if request size of the blocked writting thread can be fulfilled */
        if(thread->file_request.size <= kfifo_avail(pipe)) {
            /* yes, wake up the thread from the write waiting list */
            wake_up(w_wait_list);
        }
    }

    return retval;
}

ssize_t pipe_write_generic(pipe_t *pipe, const char *buf, size_t size)
{
    CURRENT_THREAD_INFO(curr_thread);

    ssize_t retval = 0;
    size_t fifo_avail = kfifo_avail(pipe);
    struct list *r_wait_list = &pipe->r_wait_list;

    /* check if the fifo has enough space to write or not */
    if(size > fifo_avail) {
        if(pipe->flags & O_NONBLOCK) { /* non-block mode */
            if(fifo_avail > 0) {
                kfifo_in(pipe, buf, fifo_avail);
                return fifo_avail;
            } else {
                return -EAGAIN;
            }
        } else { /* block mode */
            /* put the current thread into the read waiting list */
            prepare_to_wait(&pipe->r_wait_list, &curr_thread->list, THREAD_WAIT);
            curr_thread->file_request.size = size;

            return -ERESTARTSYS;
        }
    }

    /* push data into the pipe */
    kfifo_in(pipe, buf, size);

    /* calculate total written bytes */
    size_t type_size = kfifo_esize(pipe);
    retval = type_size * size;

    /* resume a blocked reading thread if the pipe has enough data to read */
    if(!list_is_empty(r_wait_list)) {
        /* acquire the thread info */
        struct thread_info *thread =
            list_entry(r_wait_list->next, struct thread_info, list);

        /* check if request size of the blocked reading thread can be fulfilled */
        if(thread->file_request.size <= kfifo_len(pipe)) {
            /* yes, wake up the thread from the read waiting list */
            wake_up(r_wait_list);
        }
    }

    return retval;
}

ssize_t fifo_read(struct file *filp, char *buf, size_t size, off_t offset);
ssize_t fifo_write(struct file *filp, const char *buf, size_t size, off_t offset);
int fifo_open(struct inode *inode, struct file *file);

static struct file_operations fifo_ops = {
    .read = fifo_read,
    .write = fifo_write,
    .open = fifo_open
};

int fifo_open(struct inode *inode, struct file *file)
{
    return 0;
}

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
