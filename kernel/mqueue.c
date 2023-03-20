#include <stddef.h>
#include <errno.h>
#include <string.h>
#include "fs.h"
#include "mqueue.h"
#include "mpool.h"
#include "ringbuf.h"
#include "kconfig.h"
#include "kernel.h"
#include "syscall.h"

extern struct task_ctrl_blk *running_task;
extern struct memory_pool mem_pool;

struct msg_queue mq_table[MQUEUE_CNT_MAX];
int mq_cnt = 0;

mqd_t mq_open(const char *name, int oflag, struct mq_attr *attr)
{
    /* the message queue must be in nonblock mode since it may be called in the interrupt */
    if(!(attr->mq_flags & O_NONBLOCK))
        return -1;

    /* initialize the ring buffer */
    struct ringbuf *pipe = memory_pool_alloc(&mem_pool, sizeof(struct ringbuf));
    uint8_t *pipe_mem = memory_pool_alloc(&mem_pool, attr->mq_msgsize * attr->mq_maxmsg);
    ringbuf_init(pipe, pipe_mem, attr->mq_msgsize, attr->mq_maxmsg);

    /* register a new message queue */
    int mqdes = mq_cnt;
    mq_table[mqdes].pipe = pipe;
    mq_table[mqdes].lock = 0;
    strncpy(mq_table[mqdes].name, name, FILE_NAME_LEN_MAX);
    mq_cnt++;

    /* return the message queue descriptor */
    return mqdes;
}

/* mq_receive() can only be called by user tasks */
ssize_t mq_receive(mqd_t mqdes, char *msg_ptr, size_t msg_len, unsigned int *msg_prio)
{
    struct msg_queue *mq = &mq_table[mqdes];
    struct list *task_wait_list = &mq->pipe->task_wait_list;

    ssize_t retval;

    /* start of the critical section */
    spin_lock_irq(&mq->lock);

    /* is the data in the ring buffer enough to serve the user? */
    if(msg_len > mq->pipe->count) {
        /* put the current task into the waiting list */
        prepare_to_wait(task_wait_list, &running_task->list, TASK_WAIT);

        /* update the request size */
        running_task->file_request.size = msg_len;

        retval = -EAGAIN; //ask the user to try again
    } else {
        /* pop data from the pipe */
        int i;
        for(i = 0; i < msg_len; i++) {
            ringbuf_get(mq->pipe, &msg_ptr[i]);
        }

        retval = msg_len;
    }

    /* end of the critical section */
    spin_unlock_irq(&mq->lock);

    return retval;
}

/* mq_send() can be called by user task or interrupt handler functions */
int mq_send(mqd_t mqdes, const char *msg_ptr, size_t msg_len, unsigned int msg_prio)
{
    struct msg_queue *mq = &mq_table[mqdes];
    struct list *task_wait_list = &mq->pipe->task_wait_list;

    /* start of the critical section */
    spin_lock_irq(&mq->lock);

    /* push data into the pipe */
    int i;
    for(i = 0; i < msg_len; i++) {
        ringbuf_put(mq->pipe, &msg_ptr[i]);
    }

    /* resume a blocked task if the pipe has enough data to serve */
    if(!list_is_empty(task_wait_list)) {
        struct task_ctrl_blk *task = list_entry(task_wait_list->next, struct task_ctrl_blk, list);

        /* check if the request size of the blocked task can be fulfilled */
        if(task->file_request.size <= mq->pipe->count) {
            /* wake up the task from the waiting list */
            wake_up(task_wait_list);
        }
    }

    /* end of the critical section */
    spin_unlock_irq(&mq->lock);

    return msg_len;
}
