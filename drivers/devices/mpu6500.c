#include <fs/fs.h>
#include <kernel/delay.h>
#include <printk.h>

#include "mpu6500.h"
#include "spi.h"

#define s8_to_s16(upper_s8, lower_s8) ((int16_t) upper_s8 << 8 | lower_s8)

struct mpu6500_device mpu6500 = {
    .gyro_bias = {0, 0, 0},
    .accel_bias = {0, 0, 0},
    .accel_fs = MPU6500_GYRO_FS_8G,
    .gyro_fs = MPU6500_GYRO_FS_1000_DPS,
};

int mpu6500_ioctl(struct file *filp, unsigned int cmd, unsigned long arg);
int mpu6500_open(struct inode *inode, struct file *file);

static struct file_operations mpu6500_file_ops = {
    .ioctl = mpu6500_ioctl,
    .open = mpu6500_open,
};

int mpu6500_open(struct inode *inode, struct file *file)
{
    return 0;
}

int mpu6500_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
    return 0;
}

static uint8_t mpu6500_read_byte(uint8_t address)
{
    uint8_t read;

    spi1_chip_select();
    spi1_read_write(address | 0x80);
    read = spi1_read_write(0xff);
    spi1_chip_deselect();

    return read;
}

static void mpu6500_write_byte(uint8_t address, uint8_t data)
{
    spi1_chip_select();
    spi1_read_write(address);
    spi1_read_write(data);
    spi1_chip_deselect();
}

static uint8_t mpu6500_read_who_am_i()
{
    uint8_t id = mpu6500_read_byte(MPU6500_WHO_AM_I);
    msleep(5);
    return id;
}

static void mpu6500_reset()
{
    mpu6500_write_byte(MPU6500_PWR_MGMT_1, 0x80);
    msleep(100);
}

void mpu6500_init(void)
{
    /* Probe the sensor */
    while (mpu6500_read_who_am_i() != 0x70)
        ;

    mpu6500_reset();

    switch (mpu6500.accel_fs) {
    case MPU6500_GYRO_FS_2G:
        mpu6500.accel_scale = 9.81 / 16384.0f;
        mpu6500_write_byte(MPU6500_ACCEL_CONFIG, 0x00);
        break;
    case MPU6500_GYRO_FS_4G:
        mpu6500.accel_scale = 9.81f / 8192.0f;
        mpu6500_write_byte(MPU6500_ACCEL_CONFIG, 0x08);
        break;
    case MPU6500_GYRO_FS_8G:
        mpu6500.accel_scale = 9.81f / 4096.0f;
        mpu6500_write_byte(MPU6500_ACCEL_CONFIG, 0x10);
        break;
    case MPU6500_GYRO_FS_16G:
        mpu6500.accel_scale = 9.81f / 2048.0f;
        mpu6500_write_byte(MPU6500_ACCEL_CONFIG, 0x18);
        break;
    }
    msleep(100);

    switch (mpu6500.gyro_fs) {
    case MPU6500_GYRO_FS_250_DPS:
        mpu6500.gyro_scale = 1.0f / 131.0f;
        mpu6500_write_byte(MPU6500_GYRO_CONFIG, 0x00);
        break;
    case MPU6500_GYRO_FS_500_DPS:
        mpu6500.gyro_scale = 1.0f / 65.5f;
        mpu6500_write_byte(MPU6500_GYRO_CONFIG, 0x08);
        break;
    case MPU6500_GYRO_FS_1000_DPS:
        mpu6500.gyro_scale = 1.0f / 32.8f;
        mpu6500_write_byte(MPU6500_GYRO_CONFIG, 0x10);
        break;
    case MPU6500_GYRO_FS_2000_DPS:
        mpu6500.gyro_scale = 1.0f / 16.4f;
        mpu6500_write_byte(MPU6500_GYRO_CONFIG, 0x18);
        break;
    }
    msleep(100);

    /* Gyroscope update rate = 1KHz, Low-pass filter bandwitdh = 20Hz */
    mpu6500_write_byte(MPU6500_CONFIG, GYRO_DLPF_BANDWIDTH_20Hz);
    msleep(100);

    /* Acceleromter update rate = 1KHz, Low-pass filter bandwitdh = 20Hz */
    mpu6500_write_byte(MPU6500_ACCEL_CONFIG2, ACCEL_DLPF_BANDWIDTH_20Hz);
    msleep(100);

    /* Enable data-ready interrupt */
    mpu6500_write_byte(MPU6500_INT_ENABLE, 0x01);
    msleep(100);

    register_chrdev("imu0", &mpu6500_file_ops);
    printk("imu0: mp6500");
}

void mpu6500_interrupt_handler(void)
{
    uint8_t buffer[14];

    /* Read measurements */
    spi1_chip_select();
    spi1_read_write(MPU6500_ACCEL_XOUT_H | 0x80);
    buffer[0] = spi1_read_write(0xff);
    buffer[1] = spi1_read_write(0xff);
    buffer[2] = spi1_read_write(0xff);
    buffer[3] = spi1_read_write(0xff);
    buffer[4] = spi1_read_write(0xff);
    buffer[5] = spi1_read_write(0xff);
    buffer[6] = spi1_read_write(0xff);
    buffer[7] = spi1_read_write(0xff);
    buffer[8] = spi1_read_write(0xff);
    buffer[9] = spi1_read_write(0xff);
    buffer[10] = spi1_read_write(0xff);
    buffer[11] = spi1_read_write(0xff);
    buffer[12] = spi1_read_write(0xff);
    buffer[13] = spi1_read_write(0xff);
    spi1_chip_deselect();

    /* Composite measurements */
    mpu6500.accel_unscaled[0] = -s8_to_s16(buffer[0], buffer[1]);
    mpu6500.accel_unscaled[1] = +s8_to_s16(buffer[2], buffer[3]);
    mpu6500.accel_unscaled[2] = -s8_to_s16(buffer[4], buffer[5]);
    mpu6500.temp_unscaled = s8_to_s16(buffer[6], buffer[7]);
    mpu6500.gyro_unscaled[0] = -s8_to_s16(buffer[8], buffer[9]);
    mpu6500.gyro_unscaled[1] = +s8_to_s16(buffer[10], buffer[11]);
    mpu6500.gyro_unscaled[2] = -s8_to_s16(buffer[12], buffer[13]);
}
