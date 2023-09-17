#include <errno.h>

#include <kernel/task.h>

#include <tenok/tenok.h>
#include <tenok/fcntl.h>
#include <tenok/unistd.h>
#include <tenok/mqueue.h>
#include <tenok/sys/resource.h>

#include "mavlink.h"
#include "mavlink/parser.h"
#include "mavlink/publisher.h"

mqd_t mqdes_recvd_msg;

void mavlink_task_init(void)
{
    struct mq_attr attr = {
        .mq_flags = O_NONBLOCK,
        .mq_maxmsg = 100,
        .mq_msgsize = sizeof(mavlink_message_t),
        .mq_curmsgs = 0
    };
    mqdes_recvd_msg = mq_open("/mavlink_msgs", 0, &attr);
}

void mavlink_out_task(void)
{
    mavlink_task_init();

    set_program_name("mavlink out");
    setpriority(0, getpid(), 4);

    int fd = open("/dev/serial1", 0);

    mavlink_message_t recvd_msg;

    while(1) {
        mavlink_send_heartbeat(fd);
        mavlink_send_hil_actuator_controls(fd);

        /* trigger the command parser if received new message from the queue */
        if(mq_receive(mqdes_recvd_msg, (char *)&recvd_msg, sizeof(recvd_msg), 0) == sizeof(recvd_msg)) {
            parse_mavlink_msg(&recvd_msg);
        }

        sleep(200); //5Hz
    }
}

void mavlink_in_task(void)
{
    set_program_name("mavlink in");
    setpriority(0, getpid(), 4);

    int fd = open("/dev/serial1", 0);

    uint8_t c;
    mavlink_status_t status;
    mavlink_message_t recvd_msg;

    while(1) {
        /* read byte */
        c = read(fd, &c, 1);

        /* attempt to parse the message */
        if(mavlink_parse_char(MAVLINK_COMM_1, c, &recvd_msg, &status) == 1) {
            //received, put the received message into the queue
            mq_send(mqdes_recvd_msg, (char *)&recvd_msg, sizeof(recvd_msg), 0);
        }
    }
}

HOOK_USER_TASK(mavlink_out_task);
HOOK_USER_TASK(mavlink_in_task);
