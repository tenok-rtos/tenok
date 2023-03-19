#include <stdio.h>
#include <errno.h>
#include <string.h>
#include "kernel.h"
#include "syscall.h"
#include "uart.h"
#include "mqueue.h"

mqd_t mqdes_print;

/* define your own customized message type */
typedef struct {
    char data[20];
} my_message_t;

void message_queue_task1(void)
{
    set_program_name("queue1");

    my_message_t msg;

    char *str = "hello world!\n\r";
    strncpy(msg.data, str, sizeof(msg));

    while(1) {
        mq_send(mqdes_print, (char *)&msg, 1, 0);
        sleep(200);
    }
}

void message_queue_task2(void)
{
    set_program_name("queue2");

    my_message_t msg;
    int serial_fd = open("/dev/serial0", 0, 0);

    int n = 1; //numbers of message to receive at once
    int rcvd_size;

    while(1) {
        while(mq_receive(mqdes_print, (char *)&msg, n, 0) != n);
        rcvd_size = strlen(msg.data);

        while(write(serial_fd, msg.data, rcvd_size) == EAGAIN);
    }
}

void run_mqueue_example(void)
{
    struct mq_attr attr = {
        .mq_flags = O_NONBLOCK,
        .mq_maxmsg = 100,
        .mq_msgsize = sizeof(my_message_t),
        .mq_curmsgs = 0
    };
    mqdes_print = mq_open("/my_message", 0, &attr);

    if(!fork()) message_queue_task1();
    if(!fork()) message_queue_task2();
}
