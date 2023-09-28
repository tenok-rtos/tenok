#include <tenok.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <mqueue.h>

#include <kernel/task.h>

#include "uart.h"

mqd_t mqdes_print;

/* define your own customized message type */
typedef struct {
    char data[20];
} my_message_t;

void my_mqueue_init(void)
{
    struct mq_attr attr = {
        .mq_flags = 0,
        .mq_maxmsg = 100,
        .mq_msgsize = sizeof(my_message_t),
        .mq_curmsgs = 0
    };
    mqdes_print = mq_open("/my_message", 0, &attr);
}

void message_queue_task1(void)
{
    my_mqueue_init();

    setprogname("queue1");

    my_message_t msg;

    char *str = "mqueue: greeting\n\r";
    strncpy(msg.data, str, sizeof(msg));

    while(1) {
        mq_send(mqdes_print, (char *)&msg, sizeof(msg), 0);
        sleep(1);
    }
}

void message_queue_task2(void)
{
    setprogname("queue2");

    int serial_fd = open("/dev/serial0", 0);
    my_message_t msg;

    while(1) {
        /* read message queue */
        mq_receive(mqdes_print, (char *)&msg, sizeof(msg), 0);

        /* write serial */
        write(serial_fd, msg.data, strlen(msg.data));
    }
}

HOOK_USER_TASK(message_queue_task1, 0, 512);
HOOK_USER_TASK(message_queue_task2, 0, 512);
