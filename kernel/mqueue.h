#ifndef __MQUEUE_H__
#define __MQUEUE_H__

#include "fs.h"
#include "ringbuf.h"
#include "mpool.h"

typedef int mqd_t;

struct msg_queue {
    spinlock_t lock;

    char   name[FILE_NAME_LEN_MAX];

    struct ringbuf *pipe;
};

struct mq_attr {
    int mq_flags;   //flags: 0 or O_NONBLOCK
    int mq_maxmsg;  //max number of the messages in the queue
    int mq_msgsize; //byte size of the message
    int mq_curmsgs; //number of the messages currently in the queue
};

mqd_t mq_open(const char *name, int oflag, struct mq_attr *attr);
ssize_t mq_receive(mqd_t mqdes, char *msg_ptr, size_t msg_len, unsigned int *msg_prio);
int mq_send(mqd_t mqdes, const char *msg_ptr, size_t msg_len, unsigned int msg_prio);

#endif
