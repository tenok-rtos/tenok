#include <errno.h>
#include <fcntl.h>
#include <mqueue.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>

#include <arch/port.h>
#include <common/list.h>
#include <kernel/errno.h>
#include <kernel/kernel.h>
#include <kernel/mqueue.h>
#include <kernel/syscall.h>
#include <kernel/wait.h>
#include <mm/mm.h>

struct mqueue *__mq_allocate(struct mq_attr *attr)
{
    /* allocate new message queue */
    struct mqueue *new_mq = kmalloc(sizeof(struct mqueue));

    /* allocate message buffers */
    size_t element_size = sizeof(struct mqueue_data) + attr->mq_msgsize;
    size_t buf_size = element_size * attr->mq_maxmsg;
    char *buf = kmalloc(buf_size);

    /* Failed to allocate message queue */
    if (!new_mq) {
        return NULL;
    } else if (!buf) {
        kfree(new_mq);
        return NULL;
    }

    /* Initialize message queue size and buffer */
    new_mq->size = attr->mq_maxmsg;
    new_mq->buf = buf;

    /* Initialize message queue list heads */
    INIT_LIST_HEAD(&new_mq->free_list);
    INIT_LIST_HEAD(&new_mq->r_wait_list);
    INIT_LIST_HEAD(&new_mq->w_wait_list);
    for (int i = 0; i <= MQ_PRIO_MAX; i++)
        INIT_LIST_HEAD(&new_mq->used_list[i]);

    /* Link all message buffers to the free list */
    for (int i = 0; i < new_mq->size; i++) {
        struct mqueue_data *entry =
            (struct mqueue_data *) ((uintptr_t) buf + (i * element_size));
        list_add(&entry->list, &new_mq->free_list);
    }

    /* Return the allocated message queue */
    return new_mq;
}

void __mq_free(struct mqueue *mq)
{
    kfree(mq->buf);
    kfree(mq);
}

size_t __mq_len(struct mqueue *mq)
{
    /* Return the number of the messages in the queue */
    return mq->cnt;
}

static size_t __mq_avail(struct mqueue *mq)
{
    /* Return the free space number of the queue */
    return mq->size - mq->cnt;
}

static size_t __mq_out(struct mqueue *mq, char *msg_ptr, unsigned int *msg_prio)
{
    /* Find the message list with highest prioity that contains message */
    int prio = MQ_PRIO_MAX;
    for (; prio >= 0; prio--) {
        if (!list_empty(&mq->used_list[prio]))
            break;
    }

    /* Read message from the selected list */
    struct mqueue_data *element =
        list_first_entry(&mq->used_list[prio], struct mqueue_data, list);
    memcpy(msg_ptr, element->data, element->size);
    mq->cnt--;
    if (msg_prio)
        *msg_prio = prio;

    /* Move the message from used list to the free list */
    list_move(&element->list, &mq->free_list);

    /* Return the read size */
    return element->size;
}

static void __mq_in(struct mqueue *mq,
                    const char *msg_ptr,
                    size_t msg_len,
                    unsigned int msg_prio)
{
    /* Save new message into the queue */
    struct mqueue_data *element =
        list_first_entry(&mq->free_list, struct mqueue_data, list);
    memcpy(element->data, msg_ptr, msg_len);
    element->size = msg_len;
    mq->cnt++;

    /* Move the message from free list to the used list */
    list_move(&element->list, &mq->used_list[msg_prio]);
}

ssize_t __mq_receive(struct mqueue *mq,
                     const struct mq_attr *attr,
                     char *msg_ptr,
                     size_t msg_len,
                     unsigned int *msg_prio)
{
    CURRENT_THREAD_INFO(curr_thread);

    /* The message queue descriptor is not open with reading flag */
    if ((attr->mq_flags & (0x1)) != O_RDONLY && !(attr->mq_flags & O_RDWR))
        return -EBADF;

    /* The buffer size must be larger or equal to the max message size */
    if (msg_len < attr->mq_msgsize)
        return -EMSGSIZE;

    /* Check if the queue has message to read */
    if (__mq_len(mq) <= 0) {
        if (attr->mq_flags & O_NONBLOCK) { /* Non-block mode */
            /* Return immediately */
            return -EAGAIN;
        } else { /* Block mode */
            /* Enqueue the thread into the waiting list */
            prepare_to_wait(&mq->r_wait_list, &curr_thread->list, THREAD_WAIT);
            return -ERESTARTSYS;
        }
    }

    /* Read message from the queue */
    size_t read_size = __mq_out(mq, msg_ptr, msg_prio);

    /* Wake up the highest-priority thread from the waiting list */
    wake_up(&mq->w_wait_list);

    return read_size;
}

ssize_t __mq_send(struct mqueue *mq,
                  const struct mq_attr *attr,
                  const char *msg_ptr,
                  size_t msg_len,
                  unsigned int msg_prio)
{
    CURRENT_THREAD_INFO(curr_thread);

    /* The message queue descriptor is not open with writing flag */
    if ((attr->mq_flags & (0x1)) != O_WRONLY && !(attr->mq_flags & O_RDWR))
        return -EBADF;

    /* The write size must be smaller or equal to the max message size */
    if (msg_len > attr->mq_msgsize) {
        return -EMSGSIZE;
    }

    /* Check if the queue has space to write */
    if (__mq_avail(mq) <= 0) {
        if (attr->mq_flags & O_NONBLOCK) { /* non-block mode */
            /* Return immediately */
            return -EAGAIN;
        } else { /* block mode */
            /* Enqueue the thread into the waiting list */
            prepare_to_wait(&mq->w_wait_list, &curr_thread->list, THREAD_WAIT);
            return -ERESTARTSYS;
        }
    }

    /* Save message into the queue */
    __mq_in(mq, msg_ptr, msg_len, msg_prio);

    /* Wake up the highest-priority thread from the waiting list */
    wake_up(&mq->r_wait_list);

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
