#include <errno.h>
#include <stdint.h>
#include <string.h>

#include <fs/fs.h>
#include <kernel/printk.h>
#include <kernel/time.h>

#include "sbus.h"
#include "uart.h"

static sbus_t sbus = {.rc_val = {0}, .index = 0};

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
       whether it is a new s.bus frame */
    if ((sbus.curr_time_ms - sbus.last_time_ms) > 2.0f) {
        sbus.index = 0;
    }

    if (sbus.index < 24) {
        /* Append new byte until full */
        sbus.index++;
    } else {
        /* Remove the oldest byte */
        for (int i = 0; i < 23; i++)
            sbus.buf[i] = sbus.buf[i + 1];
    }

    /* Append new byte at the last position */
    sbus.buf[sbus.index] = new_byte;

    /* Decode new frame */
    if ((sbus.index == 24) && (sbus.buf[0] == 0x0f) && (sbus.buf[24] == 0x00))
        decode_sbus(sbus.buf);

    sbus.last_time_ms = sbus.curr_time_ms;
}

static int sbus_open(struct inode *inode, struct file *file)
{
    return 0;
}

static ssize_t sbus_read(struct file *filp,
                         char *buf,
                         size_t size,
                         off_t offset)
{
    if (size != sizeof(sbus_t))
        return -EINVAL;

    memcpy(buf, &sbus, sizeof(sbus_t));
    return size;
}

static struct file_operations sbus_file_ops = {
    .read = sbus_read,
    .open = sbus_open,
};

void sbus_init(void)
{
    /* Register S.BUS receiver to the file system */
    register_chrdev("sbus", &sbus_file_ops);

    uart2_init(100000, sbus_interrupt_handler);

    printk("sbus: rc interface");
}
