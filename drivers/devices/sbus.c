#include <stdint.h>

#include <kernel/time.h>

#include "sbus.h"

sbus_t sbus;

static void decode_sbus(uint8_t *frame)
{
    sbus.rc_val[0] = ((frame[1] | frame[2] << 8) & 0x07ff);
    sbus.rc_val[1] = ((frame[2] >> 3 | frame[3] << 5) & 0x07ff);
    sbus.rc_val[2] =
        ((frame[3] >> 6 | frame[4] << 2 | frame[5] << 10) & 0x07ff);
    sbus.rc_val[3] = ((frame[5] >> 1 | frame[6] << 7) & 0x07ff);
    sbus.rc_val[4] = ((frame[6] >> 4 | frame[7] << 4) & 0x07ff);
    sbus.rc_val[5] = ((frame[7] >> 7 | frame[8] << 1 | frame[9] << 9) & 0x07ff);
    sbus.rc_val[6] = ((frame[9] >> 2 | frame[10] << 6) & 0x07ff);
    sbus.rc_val[7] = ((frame[10] >> 5 | frame[11] << 3) & 0x07ff);
    sbus.rc_val[8] = ((frame[12] | frame[13] << 8) & 0x07ff);
    sbus.rc_val[9] = ((frame[13] >> 3 | frame[14] << 5) & 0x07ff);
    sbus.rc_val[10] =
        ((frame[14] >> 6 | frame[15] << 2 | frame[16] << 10) & 0x07ff);
    sbus.rc_val[11] = ((frame[16] >> 1 | frame[17] << 7) & 0x07ff);
    sbus.rc_val[12] = ((frame[17] >> 4 | frame[18] << 4) & 0x07ff);
    sbus.rc_val[13] =
        ((frame[18] >> 7 | frame[19] << 1 | frame[20] << 9) & 0x07ff);
    sbus.rc_val[14] = ((frame[20] >> 2 | frame[21] << 6) & 0x07ff);
    sbus.rc_val[15] = ((frame[21] >> 5 | frame[22] << 3) & 0x07ff);
}

void sbus_interrupt_handler(uint8_t new_byte)
{
    sbus.curr_time_ms = ktime_get();

    /* Use reception interval time to deteminate
       whether it is a new s-bus frame */
    if ((sbus.curr_time_ms - sbus.last_time_ms) > 2.0f) {
        sbus.index = 0;
    }

    sbus.buf[sbus.index] = new_byte;
    sbus.index++;

    if ((sbus.index == 25) && (sbus.buf[0] == 0x0f) && (sbus.buf[24] == 0x00)) {
        decode_sbus(sbus.buf);
        sbus.index = 0;
    }

    sbus.last_time_ms = sbus.curr_time_ms;
}
