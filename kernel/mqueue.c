#include <errno.h>
#include <fcntl.h>
#include <mqueue.h>
#include <stddef.h>

#include <arch/port.h>
#include <kernel/errno.h>
#include <kernel/kernel.h>
#include <kernel/mqueue.h>
#include <kernel/pipe.h>
#include <kernel/syscall.h>
#include <kernel/wait.h>
#include <mm/mm.h>

struct pipe *__mq_open(struct mq_attr *attr)
{
    /* Allocate new pipe */
    struct pipe *pipe = kmalloc(sizeof(struct pipe));
    struct kfifo *pipe_fifo = kfifo_alloc(attr->mq_msgsize, attr->mq_maxmsg);

    /* Failed to allocate new pipe */
    if (pipe == NULL || pipe_fifo == NULL)
        return NULL;

    /* Initialize the pipe */
    pipe->fifo = pipe_fifo;
    INIT_LIST_HEAD(&pipe->r_wait_list);
    INIT_LIST_HEAD(&pipe->w_wait_list);

    return pipe;
}

ssize_t __mq_receive(struct pipe *pipe,
                     char *buf,
                     size_t msg_len,
                     const struct mq_attr *attr)
{
    CURRENT_THREAD_INFO(curr_thread);
    struct kfifo *fifo = pipe->fifo;

    /* The message queue descriptor is not open with reading flag */
    if ((attr->mq_flags & (0x1)) != O_RDONLY && !(attr->mq_flags & O_RDWR))
        return -EBADF;

    /* The buffer size must be larger or equal to the max message size */
    if (msg_len < attr->mq_msgsize)
        return -EMSGSIZE;

    /* Check if the FIFO has message to read */
    if (kfifo_len(fifo) <= 0) {
        if (attr->mq_flags & O_NONBLOCK) { /* Non-block mode */
            /* Return immediately */
            return -EAGAIN;
        } else { /* Block mode */
            /* Enqueue the thread into the waiting list */
            prepare_to_wait(&pipe->r_wait_list, &curr_thread->list,
                            THREAD_WAIT);
            return -ERESTARTSYS;
        }
    }

    /* Read data from the FIFO */
    size_t read_size = kfifo_peek_len(fifo);
    kfifo_out(fifo, buf, read_size);

    /* Wake up the highest-priority thread from the waiting list */
    wake_up(&pipe->w_wait_list);

    return read_size;
}

ssize_t __mq_send(struct pipe *pipe,
                  const char *buf,
                  size_t msg_len,
                  const struct mq_attr *attr)
{
    CURRENT_THREAD_INFO(curr_thread);

    struct kfifo *fifo = pipe->fifo;

    /* The message queue descriptor is not open with writing flag */
    if ((attr->mq_flags & (0x1)) != O_WRONLY && !(attr->mq_flags & O_RDWR))
        return -EBADF;

    /* The write size must be smaller or equal to the max message size */
    if (msg_len > attr->mq_msgsize) {
        return -EMSGSIZE;
    }

    /* Check if the FIFO has space to write */
    if (kfifo_avail(fifo) <= 0) {
        if (attr->mq_flags & O_NONBLOCK) { /* non-block mode */
            /* Return immediately */
            return -EAGAIN;
        } else { /* block mode */
            /* Enqueue the thread into the waiting list */
            prepare_to_wait(&pipe->w_wait_list, &curr_thread->list,
                            THREAD_WAIT);
            return -ERESTARTSYS;
        }
    }

    /* Write data to the FIFO */
    kfifo_in(fifo, buf, msg_len);

    /* Wake up the highest-priority thread from the waiting list */
    wake_up(&pipe->r_wait_list);

    return 0;
}

NACKED int mq_getattr(mqd_t mqdes, struct mq_attr *attr)
{
    SYSCALL(MQ_GETATTR);
}

NACKED int mq_setattr(mqd_t mqdes,
                      const struct mq_attr *newattr,
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

NACKED ssize_t mq_receive(mqd_t mqdes,
                          char *msg_ptr,
                          size_t msg_len,
                          unsigned int *msg_prio)
{
    SYSCALL(MQ_RECEIVE);
}

NACKED int mq_send(mqd_t mqdes,
                   const char *msg_ptr,
                   size_t msg_len,
                   unsigned int msg_prio)
{
    SYSCALL(MQ_SEND);
}
