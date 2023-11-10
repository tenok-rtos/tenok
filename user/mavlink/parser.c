#include "mavlink/hil.h"
#include "mavlink/parser.h"

struct mavlink_cmd cmd_list[] = {
    DEF_MAVLINK_CMD(mav_hil_sensor, 107),
    DEF_MAVLINK_CMD(mav_hil_gps, 113),
    DEF_MAVLINK_CMD(mav_hil_state_quaternion, 115)
};

void parse_mavlink_msg(mavlink_message_t *msg)
{
    for(int i = 0; i < (signed int)CMD_LEN(cmd_list); i++) {
        if(msg->msgid == cmd_list[i].msg_id) {
            cmd_list[i].handler(msg);
            break;
        }
    }
}
