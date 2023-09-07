#include <errno.h>
#include "mavlink.h"
#include "syscall.h"
#include "time.h"

extern int mavlink_fd;

void mavlink_send_msg(mavlink_message_t *msg, int fd)
{
    uint8_t buf[MAVLINK_MAX_PACKET_LEN];
    uint16_t len = mavlink_msg_to_send_buffer(buf, msg);

    write(fd, buf, len);
}

void mavlink_send_heartbeat(int fd)
{
    uint8_t sys_id = 1;
    uint8_t type = MAV_TYPE_QUADROTOR;
    uint8_t autopilot = MAV_AUTOPILOT_PX4;
    uint8_t base_mode = 0;
    uint32_t custom_mode = 0;
    uint8_t sys_status = 0;

    mavlink_message_t msg;
    mavlink_msg_heartbeat_pack(sys_id, 1, &msg, type, autopilot, base_mode, custom_mode, sys_status);

    mavlink_send_msg(&msg, fd);
}

void mavlink_send_hil_actuator_controls(int fd)
{
    uint8_t sys_id = 1;
    float ctrls[16] = {0.1, 0.1, 0.1, 0.1};
    uint8_t mode = MAV_MODE_FLAG_HIL_ENABLED |
                   MAV_MODE_FLAG_SAFETY_ARMED |
                   MAV_MODE_FLAG_AUTO_ENABLED;

    /* get current time */
    struct timespec tp;
    clock_gettime(CLOCK_MONOTONIC, &tp);
    uint64_t time_usec = (tp.tv_sec * 1000000) + (tp.tv_nsec / 1000);

    mavlink_message_t msg;
    mavlink_msg_hil_actuator_controls_pack(sys_id, 1, &msg, time_usec, ctrls, mode, 0);

    mavlink_send_msg(&msg, fd);
}
