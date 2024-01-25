#include <errno.h>
#include <fcntl.h>
#include <mqueue.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <task.h>
#include <tenok.h>
#include <unistd.h>

#include "uart.h"

#define MSG_SIZE_MAX 25

void message_queue_task1(void)
{
    setprogname("mqueue-ex-1");

    char *str = "greeting!";

    struct mq_attr attr = {
        .mq_maxmsg = 100,
        .mq_msgsize = MSG_SIZE_MAX,
    };

    mqd_t mqdes_print = mq_open("/my_message", O_CREAT | O_WRONLY, &attr);
    if (mqdes_print < 0) {
        exit(1);
    }

    while (1) {
        mq_send(mqdes_print, (char *) str, strlen(str), 3);
        sleep(1);
    }
}

void message_queue_task2(void)
{
    setprogname("mqueue-ex-2");

    struct mq_attr attr = {
        .mq_maxmsg = 100,
        .mq_msgsize = MSG_SIZE_MAX,
    };

    mqd_t mqdes_print = mq_open("/my_message", O_CREAT | O_RDONLY, &attr);
    if (mqdes_print < 0) {
        exit(1);
    }

    char str[MSG_SIZE_MAX] = {0};

    while (1) {
        /* Read message queue */
        unsigned int msg_prio = -1;
        mq_receive(mqdes_print, str, MSG_SIZE_MAX, &msg_prio);

        /* Print received message */
        printf("[mqueue example] received \"%s\" with priority %d\n\r", str,
               msg_prio);
    }
}

HOOK_USER_TASK(message_queue_task1, 0, STACK_SIZE_MIN);
HOOK_USER_TASK(message_queue_task2, 0, STACK_SIZE_MIN);
