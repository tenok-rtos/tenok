#ifndef __TENOK_MAVLINK_PUBLISHER_H__
#define __TENOK_MAVLINK_PUBLISHER_H__

#include "mavlink.h"

void mavlink_send_msg(mavlink_message_t *msg);
void mavlink_send_heartbeat(void);

#endif
