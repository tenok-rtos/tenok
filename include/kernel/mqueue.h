#ifndef __KERNEL_MQUEUE_H__
#define __KERNEL_MQUEUE_H__

#include <stddef.h>

#include <kernel/ipc.h>

ssize_t mq_receive_base(pipe_t *pipe, char *buf, size_t msg_len, const struct mq_attr *attr);
ssize_t mq_send_base(pipe_t *pipe, const char *buf, size_t msg_len, const struct mq_attr *attr);

#endif
