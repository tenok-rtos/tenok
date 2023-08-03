#include <errno.h>
#include "task.h"
#include "mavlink.h"
#include "syscall.h"
#include "mqueue.h"
#include "../mavlink/parser.h"
#include "../mavlink/publisher.h"

int mavlink_fd = 0;
mqd_t mqdes_recvd_msg;

void mavlink_task_init(void)
{
    mavlink_fd = open("/dev/serial1", 0, 0);

    struct mq_attr attr = {
        .mq_flags = O_NONBLOCK,
        .mq_maxmsg = 100,
        .mq_msgsize = sizeof(mavlink_message_t),
        .mq_curmsgs = 0
    };
    mqdes_recvd_msg = mq_open("/mavlink_msgs", 0, &attr);
}

char mavlink_getc(void)
{
    char c;
    while(read(mavlink_fd, &c, 1) != 1);

    return c;
}

void mavlink_out_task(void)
{
    mavlink_task_init();

    set_program_name("mavlink out");
    setpriority(0, getpid(), 4);

    mavlink_message_t recvd_msg;

    while(1) {
        mavlink_send_heartbeat();

        /* trigger the command parser if received new message from the queue */
        if(mq_receive(mqdes_recvd_msg, (char *)&recvd_msg, 1, 0) == 1) {
            parse_mavlink_msg(&recvd_msg);
        }

        sleep(200); //5Hz
    }
}

void mavlink_in_task(void)
{
    set_program_name("mavlink in");
    setpriority(0, getpid(), 4);

    uint8_t c;
    mavlink_status_t status;
    mavlink_message_t recvd_msg;

    while(1) {
        /* read byte */
        c = mavlink_getc();

        /* attempt to parse the message */
        if(mavlink_parse_char(MAVLINK_COMM_1, c, &recvd_msg, &status) == 1) {
            //received, put the received message into the queue
            mq_send(mqdes_recvd_msg, (char *)&recvd_msg, 1, 0);
        }
    }
}

HOOK_USER_TASK(mavlink_out_task);
HOOK_USER_TASK(mavlink_in_task);
