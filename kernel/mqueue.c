#include <stddef.h>
#include <fcntl.h>
#include <errno.h>
#include <mqueue.h>

#include <arch/port.h>
#include <mm/mm.h>
#include <kernel/pipe.h>
#include <kernel/wait.h>
#include <kernel/kernel.h>
#include <kernel/mqueue.h>
#include <kernel/syscall.h>
#include <kernel/errno.h>

struct pipe *__mq_open(struct mq_attr *attr)
{
    /* initialize the pipe */
    struct pipe *pipe = kmalloc(sizeof(struct pipe));
    struct kfifo *pipe_fifo =
        kfifo_alloc(attr->mq_msgsize, attr->mq_maxmsg);

    /* failed to allocate the pipe */
    if(pipe == NULL || pipe_fifo == NULL)
        return NULL;

    /* initialize the pipe object */
    pipe->fifo = pipe_fifo;
    INIT_LIST_HEAD(&pipe->r_wait_list);
    INIT_LIST_HEAD(&pipe->w_wait_list);

    return pipe;
}

ssize_t __mq_receive(struct pipe *pipe, char *buf, size_t msg_len,
                     const struct mq_attr *attr)
{
    CURRENT_THREAD_INFO(curr_thread);
    struct kfifo *fifo = pipe->fifo;

    /* the message queue descriptor is not opened for reading */
    if((attr->mq_flags & (0x1)) != O_RDONLY && !(attr->mq_flags & O_RDWR))
        return -EBADF;

    /* the buffer size must be larger or equal to the max message size */
    if(msg_len < attr->mq_msgsize)
        return -EMSGSIZE;

    /* no message available in the fifo? */
    if(kfifo_len(fifo) <= 0) {
        if(attr->mq_flags & O_NONBLOCK) { /* non-block mode */
            /* return immediately */
            return -EAGAIN;
        } else { /* block mode */
            /* force the thread to sleep */
            prepare_to_wait(&pipe->r_wait_list, &curr_thread->list,
                            THREAD_WAIT);
            return -ERESTARTSYS;
        }
    }

    /* read data from the fifo */
    size_t read_size = kfifo_peek_len(fifo);
    kfifo_out(fifo, buf, read_size);

    /* the fifo has space now, wake up a thread that waiting to write */
    wake_up(&pipe->w_wait_list);

    return read_size;
}

ssize_t __mq_send(struct pipe *pipe, const char *buf, size_t msg_len,
                  const struct mq_attr *attr)
{
    CURRENT_THREAD_INFO(curr_thread);

    struct kfifo *fifo = pipe->fifo;

    /* the message queue descriptor is not opened for writing */
    if((attr->mq_flags & (0x1)) != O_WRONLY && !(attr->mq_flags & O_RDWR))
        return -EBADF;

    /* the write size must be smaller or equal to the max message size */
    if(msg_len > attr->mq_msgsize) {
        return -EMSGSIZE;
    }

    /* fifo has no free space to write? */
    if(kfifo_avail(fifo) <= 0) {
        if(attr->mq_flags & O_NONBLOCK) { /* non-block mode */
            /* return immediately */
            return -EAGAIN;
        } else { /* block mode */
            /* force the thread to sleep */
            prepare_to_wait(&pipe->w_wait_list, &curr_thread->list,
                            THREAD_WAIT);
            return -ERESTARTSYS;
        }
    }

    /* write data to the fifo */
    kfifo_in(fifo, buf, msg_len);

    /* the fifo has message now, wake up a thread that waiting to read */
    wake_up(&pipe->r_wait_list);

    return 0;
}

NACKED int mq_getattr(mqd_t mqdes, struct mq_attr *attr)
{
    SYSCALL(MQ_GETATTR);
}

NACKED int mq_setattr(mqd_t mqdes, const struct mq_attr *newattr,
                      struct mq_attr *oldattr)
{
    SYSCALL(MQ_SETATTR);
}

NACKED mqd_t mq_open(const char *name, int oflag, struct mq_attr *attr)
{
    SYSCALL(MQ_OPEN);
}

NACKED int mq_close(mqd_t mqdes)
{
    SYSCALL(MQ_CLOSE);
}

NACKED int mq_unlink(const char *name)
{
    SYSCALL(MQ_UNLINK);
}

NACKED ssize_t mq_receive(mqd_t mqdes, char *msg_ptr, size_t msg_len, unsigned int *msg_prio)
{
    SYSCALL(MQ_RECEIVE);
}

NACKED int mq_send(mqd_t mqdes, const char *msg_ptr, size_t msg_len, unsigned int msg_prio)
{
    SYSCALL(MQ_SEND);
}
