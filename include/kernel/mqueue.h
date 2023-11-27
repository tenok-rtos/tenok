/**
 * @file
 */
#ifndef __KERNEL_MQUEUE_H__
#define __KERNEL_MQUEUE_H__

#include <stddef.h>

#include <kernel/pipe.h>

struct mqueue_data {
    struct list_head list;
    size_t size;
    char data[0];
};

struct mqueue {
    char name[NAME_MAX];
    char *buf;
    size_t size;
    size_t cnt;
    struct list_head free_list;
    struct list_head used_list[MQ_PRIO_MAX + 1];
    struct list_head r_wait_list;
    struct list_head w_wait_list;
    struct list_head list;
};

struct mq_desc {
    struct mqueue *mq;
    struct mq_attr attr;
};

struct mqueue *__mq_allocate(struct mq_attr *attr);
void __mq_free(struct mqueue *mq);
size_t __mq_len(struct mqueue *mq);
ssize_t __mq_receive(struct mqueue *mq,
                     char *buf,
                     size_t msg_len,
                     unsigned int *priority,
                     const struct mq_attr *attr);
ssize_t __mq_send(struct mqueue *mq,
                  const char *buf,
                  size_t msg_len,
                  unsigned int priority,
                  const struct mq_attr *attr);

#endif
