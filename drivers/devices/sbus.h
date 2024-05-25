#ifndef __SBUS_H__
#define __SBUS_H__

#include <stdint.h>

#include <kernel/time.h>

/* TODO: Stored in adjustable parameter list */
#define RC_THROTTLE_MAX 1693
#define RC_THROTTLE_MIN 306

#define RC_ROLL_MAX 1693
#define RC_ROLL_MIN 306

#define RC_PITCH_MAX 1693
#define RC_PITCH_MIN 306

#define RC_YAW_MAX 1693
#define RC_YAW_MIN 306

#define RC_SAFETY_MAX 1694
#define RC_SAFETY_MIN 306

#define RC_THROTTLE_RANGE_MAX 100.0f
#define RC_THROTTLE_RANGE_MIN 0.0f

#define RC_ROLL_RANGE_MAX +35.0f
#define RC_ROLL_RANGE_MIN -35.0f

#define RC_PITCH_RANGE_MAX +35.0f
#define RC_PITCH_RANGE_MIN -35.0f

#define RC_YAW_RANGE_MAX +35.0f
#define RC_YAW_RANGE_MIN -35.0f

#define RC_SAFETY_THRESHOLD ((float) (RC_SAFETY_MAX - RC_SAFETY_MIN) / 2.0)

typedef struct {
    uint8_t buf[25];
    uint8_t index;
    uint16_t rc_val[16];

    float throttle;
    float roll;
    float pitch;
    float yaw;
    bool dual_switch1;

    ktime_t curr_time_ms;
    ktime_t last_time_ms;
} sbus_t;

void sbus_init(void);
void sbus_interrupt_handler(uint8_t new_byte);

#endif
