#include "mavlink.h"

void mav_hil_sensor(mavlink_message_t *msg)
{
    mavlink_hil_sensor_t hil_sensor;
    mavlink_msg_hil_sensor_decode(msg, &hil_sensor);
}

void mav_hil_gps(mavlink_message_t *msg)
{
    mavlink_hil_gps_t hil_gps;
    mavlink_msg_hil_gps_decode(msg, &hil_gps);
}

void mav_hil_state_quaternion(mavlink_message_t *msg)
{
    mavlink_hil_state_quaternion_t hil_state_q;
    mavlink_msg_hil_state_quaternion_decode(msg, &hil_state_q);
}
