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

#define MSG_LEN(mq)   (mq->attr.mq_msgsize)
#define MSG_FLAGS(mq) (mq->attr.mq_flags)

extern struct task_ctrl_blk *running_task;
extern struct memory_pool mem_pool;

struct msg_queue mq_table[MQUEUE_CNT_MAX];
int mq_cnt = 0;

mqd_t mq_open(const char *name, int oflag, struct mq_attr *attr)
{
    if(mq_cnt >= MQUEUE_CNT_MAX) {
        return -1;
    }

    /* initialize the ring buffer */
    struct ringbuf *pipe = memory_pool_alloc(&mem_pool, sizeof(struct ringbuf));
    uint8_t *pipe_mem = memory_pool_alloc(&mem_pool, attr->mq_msgsize * attr->mq_maxmsg);
    ringbuf_init(pipe, pipe_mem, attr->mq_msgsize, attr->mq_maxmsg);

    /* register a new message queue */
    int mqdes = mq_cnt;
    mq_table[mqdes].pipe = pipe;
    mq_table[mqdes].lock = 0;
    mq_table[mqdes].attr = *attr;
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

    ssize_t retval = 0;
    bool sleep = false;

    /* buffer size (msg_len) must be equal or larger than the message size */
    if(msg_len < MSG_LEN(mq)) {
        return -EMSGSIZE;
    }

    bool _sleep;

    do {
        /* start of the critical section */
        spin_lock_irq(&mq->lock);

        /* does the ring buffer contain any message?  */
        if(mq->pipe->count <= 0) {
            if(MSG_FLAGS(mq) & O_NONBLOCK) {
                retval = -EAGAIN; //ask the user to try again
                _sleep = false;
            } else {
                /* put the current task into the waiting list */
                prepare_to_wait(task_wait_list, &running_task->list, TASK_WAIT);
                _sleep = true;
            }
        } else {
            /* yes, pop a message from the ring buffer */
            ringbuf_get(mq->pipe, msg_ptr);

            /* message is acquired and ready to leave */
            retval = MSG_LEN(mq); //numbers of the received bytes
            _sleep = false;
        }

        /* end of the critical section */
        spin_unlock_irq(&mq->lock);

        /* no message is obtained under the blocked mode, go sleep */
        if(_sleep && (get_proc_mode() == 0)) {
            sched_yield();
        }
    } while(_sleep);

    return retval;
}

/* mq_send() can be called by user task or interrupt handler functions */
int mq_send(mqd_t mqdes, const char *msg_ptr, size_t msg_len, unsigned int msg_prio)
{
    struct msg_queue *mq = &mq_table[mqdes];
    struct list *task_wait_list = &mq->pipe->task_wait_list;

    /* buffer size (msg_len) must be equal or larger than the message size */
    if(msg_len < MSG_LEN(mq)) {
        return -EMSGSIZE;
    }

    /* start of the critical section */
    spin_lock_irq(&mq->lock);

    /* push a message into the pipe */
    ringbuf_put(mq->pipe, msg_ptr);

    /* wake up a blocked task if the pipe contains messages */
    if(!list_is_empty(task_wait_list)) {
        struct task_ctrl_blk *task = list_entry(task_wait_list->next, struct task_ctrl_blk, list);

        /* check if the ring buffer contains any message */
        if(mq->pipe->count > 0) {
            /* yes, wake up the task from the waiting list */
            wake_up(task_wait_list);
        }
    }

    /* end of the critical section */
    spin_unlock_irq(&mq->lock);

    return 0;
}
