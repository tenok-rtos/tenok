#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include <fs/fs.h>
#include <kernel/errno.h>
#include <kernel/kernel.h>
#include <kernel/kfifo.h>
#include <kernel/pipe.h>
#include <kernel/poll.h>
#include <kernel/wait.h>
#include <mm/mm.h>

ssize_t fifo_read(struct file *filp, char *buf, size_t size, off_t offset);
ssize_t fifo_write(struct file *filp,
                   const char *buf,
                   size_t size,
                   off_t offset);
int fifo_open(struct inode *inode, struct file *file);

static struct file_operations fifo_ops = {
    .read = fifo_read,
    .write = fifo_write,
    .open = fifo_open,
};

int fifo_init(int fd,
              struct file **files,
              struct inode *file_inode,
              struct pipe *pipe)
{
    /* initialize the pipe object */
    INIT_LIST_HEAD(&pipe->r_wait_list);
    INIT_LIST_HEAD(&pipe->w_wait_list);

    /* register fifo to the file table */
    memset(&pipe->file, 0, sizeof(pipe->file));
    pipe->file.f_op = &fifo_ops;
    pipe->file.f_inode = file_inode;
    files[fd] = &pipe->file;

    return 0;
}

int fifo_open(struct inode *inode, struct file *file)
{
    return 0;
}

static void fifo_wake_up(struct list_head *wait_list, size_t avail_size)
{
    struct thread_info *highest_pri_thread = NULL;

    struct list_head *curr, *next;
    list_for_each_safe (curr, next, wait_list) {
        struct thread_info *thread = list_entry(curr, struct thread_info, list);

        if (thread->file_request_size <= avail_size &&
            (highest_pri_thread == NULL ||
             thread->priority > highest_pri_thread->priority)) {
            highest_pri_thread = thread;
        }
    }

    if (highest_pri_thread)
        finish_wait(&highest_pri_thread->list);
}

static ssize_t __fifo_read(struct file *filp, char *buf, size_t size)
{
    CURRENT_THREAD_INFO(curr_thread);

    struct pipe *pipe = container_of(filp, struct pipe, file);
    struct kfifo *fifo = pipe->fifo;
    size_t fifo_len = kfifo_len(fifo);

    /* check if the request size is larger than the fifo can serve */
    if (size > fifo_len) {
        if (filp->f_flags & O_NONBLOCK) { /* non-block mode */
            if (fifo_len > 0) {
                /* overwrite the read size */
                size = fifo_len;
            } else {
                return -EAGAIN;
            }
        } else { /* block mode */
            /* save the read request size */
            curr_thread->file_request_size = size;

            /* force the thread to sleep */
            prepare_to_wait(&pipe->r_wait_list, &curr_thread->list,
                            THREAD_WAIT);
            return -ERESTARTSYS;
        }
    }

    /* pop data from the pipe */
    for (int i = 0; i < size; i++)
        kfifo_out(fifo, &buf[i], sizeof(char));

    /* wake up the highest priority thread such that its
     * write request can be served */
    fifo_wake_up(&pipe->w_wait_list, kfifo_avail(fifo));

    return size;
}

static ssize_t __fifo_write(struct file *filp, const char *buf, size_t size)
{
    CURRENT_THREAD_INFO(curr_thread);

    struct pipe *pipe = container_of(filp, struct pipe, file);
    struct kfifo *fifo = pipe->fifo;
    size_t fifo_avail = kfifo_avail(fifo);

    /* check if the fifo has enough space to write or not */
    if (size > fifo_avail) {
        if (filp->f_flags & O_NONBLOCK) { /* non-block mode */
            if (fifo_avail > 0) {
                /* overwrite the write size */
                size = fifo_avail;
            } else {
                return -EAGAIN;
            }
        } else { /* block mode */
            /* save the write request size */
            curr_thread->file_request_size = size;

            /* force the thread to sleep */
            prepare_to_wait(&pipe->w_wait_list, &curr_thread->list,
                            THREAD_WAIT);
            return -ERESTARTSYS;
        }
    }

    /* push data into the pipe */
    for (int i = 0; i < size; i++)
        kfifo_in(fifo, &buf[i], sizeof(char));

    /* wake up the highest priority thread such that its
     * read request can be served */
    fifo_wake_up(&pipe->r_wait_list, kfifo_len(fifo));

    return size;
}

ssize_t fifo_read(struct file *filp, char *buf, size_t size, off_t offset)
{
    ssize_t retval = __fifo_read(filp, buf, size);

    /* update file event */
    struct pipe *pipe = container_of(filp, struct pipe, file);
    if (kfifo_len(pipe->fifo) > 0) {
        filp->f_events |= POLLOUT;
        poll_notify(filp);
    } else {
        filp->f_events &= ~POLLOUT;
    }

    return retval;
}

ssize_t fifo_write(struct file *filp,
                   const char *buf,
                   size_t size,
                   off_t offset)
{
    ssize_t retval = __fifo_write(filp, buf, size);

    /* update file event */
    struct pipe *pipe = container_of(filp, struct pipe, file);
    if (kfifo_avail(pipe->fifo) > 0) {
        filp->f_events |= POLLIN;
        poll_notify(filp);
    } else {
        filp->f_events &= ~POLLIN;
    }

    return retval;
}
