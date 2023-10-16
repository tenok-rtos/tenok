#ifndef __KERNEL_MQUEUE_H__
#define __KERNEL_MQUEUE_H__

#include <stddef.h>

#include <kernel/ipc.h>

struct mqueue {
    char name[FILE_NAME_LEN_MAX];
    pipe_t *pipe;
    struct mq_attr attr;
    struct list_head list;
};

struct mq_desc {
    struct mqueue *mq;
};

ssize_t mq_receive_base(pipe_t *pipe, char *buf, size_t msg_len, const struct mq_attr *attr);
ssize_t mq_send_base(pipe_t *pipe, const char *buf, size_t msg_len, const struct mq_attr *attr);

#endif
