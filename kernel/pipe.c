#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <stdbool.h>
#include <stddef.h>
#include <string.h>
#include <sys/syslimits.h>
#include <sys/types.h>

#include <common/list.h>
#include <fs/fs.h>
#include <kernel/errno.h>
#include <kernel/kernel.h>
#include <kernel/kfifo.h>
#include <kernel/pipe.h>
#include <kernel/poll.h>
#include <kernel/preempt.h>
#include <kernel/thread.h>
#include <kernel/wait.h>
#include <mm/mm.h>

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
        finish_wait(highest_pri_thread);
}

static ssize_t __fifo_read(struct file *filp, char *buf, size_t size)
{
    CURRENT_THREAD_INFO(curr_thread);

    struct pipe *pipe = container_of(filp, struct pipe, file);
    struct kfifo *fifo = pipe->fifo;
    size_t fifo_len = kfifo_len(fifo);

    /* A stream ends where its last writer left it */
    if (pipe->stream && fifo_len == 0 && pipe->writers == 0)
        return 0;

    /* Check if the request size is larger than the FIFO can serve */
    if (size > fifo_len) {
        if (filp->f_flags & O_NONBLOCK) { /* Non-block mode */
            if (fifo_len > 0) {
                /* Set the read size to the largest amount of available size */
                size = fifo_len;
            } else {
                return -EAGAIN;
            }
        } else if (pipe->stream && fifo_len > 0) {
            /* A stream gives what it holds instead of waiting for the rest */
            size = fifo_len;
        } else { /* Block mode */
            /* Save the read request size. A stream is served by any byte at
             * all, a message only by the whole of it
             */
            curr_thread->file_request_size = pipe->stream ? 1 : size;

            /* Enqueue the thread into the waiting list */
            prepare_to_wait(&pipe->r_wait_list, curr_thread, THREAD_WAIT);
            return -ERESTARTSYS;
        }
    }

    /* Pop data from the pipe */
    for (int i = 0; i < size; i++)
        kfifo_out(fifo, &buf[i], sizeof(char));

    /* Wake up the highest-priority thread */
    fifo_wake_up(&pipe->w_wait_list, kfifo_avail(fifo));

    return size;
}

static ssize_t __fifo_write(struct file *filp, const char *buf, size_t size)
{
    CURRENT_THREAD_INFO(curr_thread);

    struct pipe *pipe = container_of(filp, struct pipe, file);
    struct kfifo *fifo = pipe->fifo;
    size_t fifo_avail = kfifo_avail(fifo);

    /* Nobody is left to read what would be written to the stream */
    if (pipe->stream && pipe->readers == 0)
        return -EPIPE;

    /* Check if the FIFO has enough space to write or not */
    if (size > fifo_avail) {
        if (filp->f_flags & O_NONBLOCK) { /* Non-block mode */
            if (fifo_avail > 0) {
                /* Set the write size to the largest amount of available size */
                size = fifo_avail;
            } else {
                return -EAGAIN;
            }
        } else { /* Block mode */
            /* Save the write request size */
            curr_thread->file_request_size = size;

            /* Enqueue the thread into the waiting list */
            prepare_to_wait(&pipe->w_wait_list, curr_thread, THREAD_WAIT);
            return -ERESTARTSYS;
        }
    }

    /* Push data into the pipe */
    for (int i = 0; i < size; i++)
        kfifo_in(fifo, &buf[i], sizeof(char));

    /* Wake up the highest-priority thread */
    fifo_wake_up(&pipe->r_wait_list, kfifo_len(fifo));

    return size;
}

ssize_t fifo_read(struct file *filp, char *buf, size_t size, off_t offset)
{
    preempt_disable();

    ssize_t retval = __fifo_read(filp, buf, size);

    /* Update file events */
    struct pipe *pipe = container_of(filp, struct pipe, file);
    if (kfifo_len(pipe->fifo) > 0) {
        filp->f_events |= POLLIN;
        poll_notify(filp);
    } else {
        filp->f_events &= ~POLLIN;
    }

    preempt_enable();

    return retval;
}

ssize_t fifo_write(struct file *filp,
                   const char *buf,
                   size_t size,
                   off_t offset)
{
    preempt_disable();

    ssize_t retval = __fifo_write(filp, buf, size);

    /* Update file events */
    struct pipe *pipe = container_of(filp, struct pipe, file);
    if (kfifo_avail(pipe->fifo) > 0) {
        filp->f_events |= POLLOUT;
        poll_notify(filp);
    } else {
        filp->f_events &= ~POLLOUT;
    }

    preempt_enable();

    return retval;
}

static struct file_operations fifo_ops = {
    .read = fifo_read,
    .write = fifo_write,
    .open = fifo_open,
};

struct file *fifo_init(struct inode *file_inode, struct pipe *pipe)
{
    /* Initialize the pipe */
    INIT_LIST_HEAD(&pipe->r_wait_list);
    INIT_LIST_HEAD(&pipe->w_wait_list);

    pipe->stream = false;
    pipe->readers = 0;
    pipe->writers = 0;

    memset(&pipe->file, 0, sizeof(pipe->file));
    pipe->file.f_op = &fifo_ops;
    pipe->file.f_inode = file_inode;

    return &pipe->file;
}

struct file *pipe_alloc(void)
{
    struct pipe *pipe = kmalloc(sizeof(struct pipe));
    struct kfifo *fifo = kfifo_alloc(1, PIPE_BUF);

    if (!pipe || !fifo) {
        if (pipe)
            kfree(pipe);
        if (fifo)
            kfifo_free(fifo);

        return NULL;
    }

    pipe->fifo = fifo;

    /* A pipe has no name, so it has no inode to be named through */
    struct file *filp = fifo_init(NULL, pipe);

    /* What separates a pipe from the message pipes of the kernel, which
     * fifo_init() leaves every pipe as
     */
    pipe->stream = true;

    return filp;
}

bool file_is_pipe(struct file *filp)
{
    /* Only a pipe is asked through these operations, and only a file that sits
     * inside one has a flag to be read
     */
    if (!filp || filp->f_op != &fifo_ops)
        return false;

    return container_of(filp, struct pipe, file)->stream;
}

/* Releases a pipe no descriptor ever named */
void pipe_release(struct file *filp)
{
    struct pipe *pipe = container_of(filp, struct pipe, file);

    kfifo_free(pipe->fifo);
    kfree(pipe);
}

/* Takes one end of the pipe, which a descriptor is handed out to name */
void pipe_take(struct file *filp, bool writer)
{
    struct pipe *pipe = container_of(filp, struct pipe, file);

    if (writer)
        pipe->writers++;
    else
        pipe->readers++;
}

/* Gives up one end of the pipe and releases it once both have been given up */
void pipe_give_up(struct file *filp, bool writer)
{
    struct pipe *pipe = container_of(filp, struct pipe, file);

    if (writer) {
        if (pipe->writers > 0)
            pipe->writers--;

        /* A reader waiting for a byte that will never come has to be told
         * that the stream has ended instead
         */
        if (pipe->writers == 0)
            fifo_wake_up(&pipe->r_wait_list, kfifo_size(pipe->fifo));
    } else {
        if (pipe->readers > 0)
            pipe->readers--;

        if (pipe->readers == 0)
            fifo_wake_up(&pipe->w_wait_list, kfifo_size(pipe->fifo));
    }

    if (pipe->readers == 0 && pipe->writers == 0)
        pipe_release(filp);
}
