#ifndef __QUADROTOR_H__
#define __QUADROTOR_H__

#include <stdbool.h>

bool is_flight_ctrl_running(void);
void trigger_esc_calibration(void);

#endif
