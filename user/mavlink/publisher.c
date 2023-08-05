#include <errno.h>
#include "mavlink.h"
#include "syscall.h"

extern int mavlink_fd;

void mavlink_send_msg(mavlink_message_t *msg)
{
    uint8_t buf[MAVLINK_MAX_PAYLOAD_LEN];
    uint16_t len = mavlink_msg_to_send_buffer(buf, msg);

    while(write(mavlink_fd, buf, len) == -EAGAIN);
}

void mavlink_send_heartbeat(void)
{
    uint8_t sys_id = 1;
    uint8_t type = MAV_TYPE_QUADROTOR;
    uint8_t autopilot = MAV_AUTOPILOT_PX4;
    uint8_t base_mode = 0;
    uint32_t custom_mode = 0;
    uint8_t sys_status = 0;

    mavlink_message_t msg;
    mavlink_msg_heartbeat_pack(sys_id, 1, &msg, type, autopilot, base_mode, custom_mode, sys_status);

    mavlink_send_msg(&msg);
}

void mavlink_send_hil_actuator_controls(void)
{
    uint8_t sys_id = 1;
    float ctrls[16] = {1};
    uint8_t mode = MAV_MODE_FLAG_HIL_ENABLED |
                   MAV_MODE_FLAG_SAFETY_ARMED |
                   MAV_MODE_FLAG_AUTO_ENABLED;

    mavlink_message_t msg;
    mavlink_msg_hil_actuator_controls_pack(sys_id, 1, &msg, 0, ctrls, mode, 0);

    mavlink_send_msg(&msg);
}
