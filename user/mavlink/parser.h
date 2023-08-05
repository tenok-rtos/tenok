#ifndef __TENOK_MAVLINK_PARSER_H
#define __TENOK_MAVLINK_PARSER_H

#include <stdint.h>
#include "mavlink.h"

#define DEF_MAVLINK_CMD(handler_function, id) \
	[handler_function ## _ID] = {.handler = handler_function, .msg_id = id}

#define ENUM_MAVLINK_HANDLER(handler_function) handler_function ## _ID

#define CMD_LEN(list) (sizeof(list) / sizeof(struct mavlink_cmd))

struct mavlink_cmd {
    uint16_t msg_id;
    void (*handler)(mavlink_message_t *msg);
};

void parse_mavlink_msg(mavlink_message_t *msg);

#endif
