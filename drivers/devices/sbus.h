#ifndef __SBUS_H__
#define __SBUS_H__

#include <stdint.h>

#include <kernel/time.h>

typedef struct {
    uint8_t buf[25];
    uint8_t index;
    uint16_t rc_val[16];
    ktime_t curr_time_ms;
    ktime_t last_time_ms;
} sbus_t;

void sbus_init(void);
void sbus_interrupt_handler(uint8_t new_byte);

#endif
