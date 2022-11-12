#ifndef __MQUEUE_H__
#define __MQUEUE_H__

#include "file.h"
#include "ringbuf.h"
#include "mpool.h"

typedef int mqd_t;

struct mq_attr {
	long mq_flags;   //flags: 0 or O_NONBLOCK
	long mq_maxmsg;  //max number of the messages in the queue
	long mq_msgsize; //byte size of the message
	long mq_curmsgs; //number of the messages currently in the queue
};

int mq_init(int fd, struct file **files, struct mq_attr *attr, struct memory_pool *mem_pool);

#endif
