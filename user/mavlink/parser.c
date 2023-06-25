#include "./parser.h"

struct mavlink_cmd *cmd_list;
int msg_list_size = 0;

void parse_mavlink_msg(mavlink_message_t *msg)
{
    int i;
    for(i = 0; i < msg_list_size; i++) {
        if(msg->msgid == cmd_list[i].msg_id) {
            cmd_list[i].handler(msg);
            break;
        }
    }
}
