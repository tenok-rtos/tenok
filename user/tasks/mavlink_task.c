#include "task.h"
#include "mavlink.h"
#include "syscall.h"

void mavlink_out_task(void)
{
    set_program_name("mavlink out");
    setpriority(0, getpid(), 4);

    uint8_t sys_id = 0;
    uint8_t type = MAV_TYPE_QUADROTOR;
    uint8_t autopilot = MAV_AUTOPILOT_PX4;
    uint8_t base_mode = 0;
    uint32_t custom_mode = 0;
    uint8_t sys_status = 0;

    mavlink_message_t msg;
    mavlink_msg_heartbeat_pack(sys_id, 1, &msg, type, autopilot, base_mode, custom_mode, sys_status);

    uint8_t buf[MAVLINK_MAX_PAYLOAD_LEN];
    uint16_t len = mavlink_msg_to_send_buffer(buf, &msg);

    while(1) {
        sleep(1000);
    }
}

void mavlink_in_task(void)
{
    set_program_name("mavlink in");
    setpriority(0, getpid(), 4);

    while(1) {
        sleep(1000);
    }
}

HOOK_USER_TASK(mavlink_out_task);
HOOK_USER_TASK(mavlink_in_task);
