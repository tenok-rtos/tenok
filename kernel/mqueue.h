#ifndef __MQUEUE_H__
#define __MQUEUE_H__

#include "file.h"
#include "ringbuf.h"
#include "mpool.h"

typedef int mqd_t;

struct mq_attr {
	int mq_flags;   //flags: 0 or O_NONBLOCK
	int mq_maxmsg;  //max number of the messages in the queue
	int mq_msgsize; //byte size of the message
	int mq_curmsgs; //number of the messages currently in the queue
};

int mqueue_init(int fd, struct file **files, struct mq_attr *attr, struct memory_pool *mem_pool);
ssize_t mqueue_receive(struct file *filp, char *msg_ptr, size_t msg_len, unsigned int msg_prio);
ssize_t mqueue_send(struct file *filp, const char *msg_ptr, size_t msg_len, unsigned int msg_prio);

#endif
