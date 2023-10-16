#include <mqueue.h>

#include <arch/port.h>
#include <kernel/syscall.h>

NACKED int mq_getattr(mqd_t mqdes, struct mq_attr *attr)
{
    SYSCALL(MQ_GETATTR);
}

NACKED int mq_setattr(mqd_t mqdes, const struct mq_attr *newattr,
                      struct mq_attr *oldattr)
{
    SYSCALL(MQ_SETATTR);
}

NACKED mqd_t mq_open(const char *name, int oflag, struct mq_attr *attr)
{
    SYSCALL(MQ_OPEN);
}

NACKED int mq_close(mqd_t mqdes)
{
    SYSCALL(MQ_CLOSE);
}

NACKED int mq_unlink(const char *name)
{
    SYSCALL(MQ_UNLINK);
}

NACKED ssize_t mq_receive(mqd_t mqdes, char *msg_ptr, size_t msg_len, unsigned int *msg_prio)
{
    SYSCALL(MQ_RECEIVE);
}

NACKED int mq_send(mqd_t mqdes, const char *msg_ptr, size_t msg_len, unsigned int msg_prio)
{
    SYSCALL(MQ_SEND);
}
