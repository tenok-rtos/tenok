#ifndef __TENOK_MAVLINK_HIL_H__
#define __TENOK_MAVLINK_HIL_H__

#include "mavlink.h"

void mav_hil_sensor(mavlink_message_t *msg);
void mav_hil_gps(mavlink_message_t *msg);
void mav_hil_state_quaternion(mavlink_message_t *msg);

#endif
