/**
 * @file
 */
#ifndef __KERNEL_MQUEUE_H__
#define __KERNEL_MQUEUE_H__

#include <stddef.h>

#include <kernel/pipe.h>

struct mqueue {
    char name[FILE_NAME_LEN_MAX];
    struct pipe *pipe;
    struct list_head list;
};

struct mq_desc {
    struct mqueue *mq;
    struct mq_attr attr;
};

struct pipe *__mq_open(struct mq_attr *attr);
ssize_t __mq_receive(struct pipe *pipe,
                     char *buf,
                     size_t msg_len,
                     const struct mq_attr *attr);
ssize_t __mq_send(struct pipe *pipe,
                  const char *buf,
                  size_t msg_len,
                  const struct mq_attr *attr);

#endif
