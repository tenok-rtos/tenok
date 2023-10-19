#ifndef __KERNEL_MQUEUE_H__
#define __KERNEL_MQUEUE_H__

#include <stddef.h>

#include <kernel/ipc.h>

#define MQD_INDEX(mqd) (mqd & 0xffff) /* [15:0] */
#define MQD_OWNER(mqd) ((mqd >> 16) & 0xff) /* [23:16] */

#define MQD_INIT(pid, mqd_idx) (((uint8_t)pid << 16) | ((uint16_t)mqd_idx))

struct mqueue {
    char name[FILE_NAME_LEN_MAX];
    pipe_t *pipe;
    struct list_head list;
};

struct mq_desc {
    struct mqueue *mq;
    struct mq_attr attr;
};

ssize_t __mq_receive(pipe_t *pipe, char *buf, size_t msg_len, const struct mq_attr *attr);
ssize_t __mq_send(pipe_t *pipe, const char *buf, size_t msg_len, const struct mq_attr *attr);

#endif
