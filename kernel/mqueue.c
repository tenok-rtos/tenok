#include <arch/port.h>
#include <kernel/syscall.h>

#include <tenok/mqueue.h>

NACKED mqd_t mq_open(const char *name, int oflag, struct mq_attr *attr)
{
    SYSCALL(MQ_OPEN);
}

NACKED ssize_t mq_receive(mqd_t mqdes, char *msg_ptr, size_t msg_len, unsigned int *msg_prio)
{
    SYSCALL(MQ_RECEIVE);
}

NACKED int mq_send(mqd_t mqdes, const char *msg_ptr, size_t msg_len, unsigned int msg_prio)
{
    SYSCALL(MQ_SEND);
}
