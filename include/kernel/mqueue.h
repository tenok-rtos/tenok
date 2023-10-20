#ifndef __KERNEL_MQUEUE_H__
#define __KERNEL_MQUEUE_H__

#include <stddef.h>

#include <kernel/pipe.h>

struct mqueue {
    char name[FILE_NAME_LEN_MAX];
    pipe_t *pipe;
    struct list_head list;
};

struct mq_desc {
    struct mqueue *mq;
    struct mq_attr attr;
};

pipe_t *__mq_open(struct mq_attr *attr);
ssize_t __mq_receive(pipe_t *pipe, char *buf, size_t msg_len, const struct mq_attr *attr);
ssize_t __mq_send(pipe_t *pipe, const char *buf, size_t msg_len, const struct mq_attr *attr);

#endif
