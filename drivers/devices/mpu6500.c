#include <errno.h>
#include <string.h>

#include <fs/fs.h>
#include <kernel/delay.h>
#include <kernel/preempt.h>
#include <printk.h>

#include "lpf.h"
#include "mpu6500.h"
#include "spi.h"
#include "stm32f4xx_conf.h"

#define s8_to_s16(upper_s8, lower_s8) ((int16_t) upper_s8 << 8 | lower_s8)

#define MPU6500_EXTI_ISR_PRIORITY 14

static struct mpu6500_device mpu6500 = {
    .accel_fs = MPU6500_GYRO_FS_8G,
    .gyro_fs = MPU6500_GYRO_FS_1000_DPS,
};

/* First order low-pass filter for acceleromter */
static float mpu6500_lpf_gain;

static int mpu6500_accel_open(struct inode *inode, struct file *file)
{
    return 0;
}

static ssize_t mpu6500_accel_read(struct file *filp,
                                  char *buf,
                                  size_t size,
                                  off_t offset)
{
    if (size != sizeof(float[3]))
        return -EINVAL;

    preempt_disable();
    memcpy(buf, mpu6500.accel_lpf, sizeof(float[3]));
    preempt_enable();

    return size;
}

static struct file_operations mpu6500_accel_fops = {
    .read = mpu6500_accel_read,
    .open = mpu6500_accel_open,
};

static int mpu6500_gyro_open(struct inode *inode, struct file *file)
{
    return 0;
}

static ssize_t mpu6500_gyro_read(struct file *filp,
                                 char *buf,
                                 size_t size,
                                 off_t offset)
{
    if (size != sizeof(float[3]))
        return -EINVAL;

    preempt_disable();
    memcpy(buf, mpu6500.gyro_raw, sizeof(float[3]));
    preempt_enable();

    return size;
}

static struct file_operations mpu6500_gyro_fops = {
    .read = mpu6500_gyro_read,
    .open = mpu6500_gyro_open,
};

static void mpu6500_interrupt_init(void)
{
    RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOD, ENABLE);
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_SYSCFG, ENABLE);

    GPIO_InitTypeDef GPIO_InitStruct = {
        .GPIO_Mode = GPIO_Mode_IN,
        .GPIO_OType = GPIO_OType_PP,
        .GPIO_Pin = GPIO_Pin_10,
        .GPIO_PuPd = GPIO_PuPd_DOWN,
    };
    GPIO_Init(GPIOD, &GPIO_InitStruct);

    SYSCFG_EXTILineConfig(EXTI_PortSourceGPIOD, EXTI_PinSource10);
    EXTI_InitTypeDef EXTI_InitStruct = {
        .EXTI_Line = EXTI_Line10,
        .EXTI_LineCmd = ENABLE,
        .EXTI_Mode = EXTI_Mode_Interrupt,
        .EXTI_Trigger = EXTI_Trigger_Rising,
    };
    EXTI_Init(&EXTI_InitStruct);

    NVIC_InitTypeDef NVIC_InitStruct = {
        .NVIC_IRQChannel = EXTI15_10_IRQn,
        .NVIC_IRQChannelPreemptionPriority = MPU6500_EXTI_ISR_PRIORITY,
        .NVIC_IRQChannelSubPriority = 0,
        .NVIC_IRQChannelCmd = ENABLE,
    };
    NVIC_Init(&NVIC_InitStruct);
}

static inline void mpu6500_spi_init(void)
{
    spi1_init();
}

static inline uint8_t mpu6500_spi_w8r8(uint8_t data)
{
    return spi1_w8r8(data);
}

static inline void mpu6500_spi_set_chipselect(bool chipselect)
{
    spi1_set_chipselect(chipselect);
}

static uint8_t mpu6500_read_byte(uint8_t address)
{
    uint8_t read;

    mpu6500_spi_set_chipselect(true);
    mpu6500_spi_w8r8(address | 0x80);
    read = mpu6500_spi_w8r8(0xff);
    mpu6500_spi_set_chipselect(false);

    return read;
}

static void mpu6500_write_byte(uint8_t address, uint8_t data)
{
    mpu6500_spi_set_chipselect(true);
    mpu6500_spi_w8r8(address);
    mpu6500_spi_w8r8(data);
    mpu6500_spi_set_chipselect(false);
}

void __msleep(uint32_t ms)
{
    volatile uint32_t cnt;
    for (cnt = ms; cnt > 0; cnt--) {
        volatile uint32_t tweaked_delay = 22500U;
        while (tweaked_delay--)
            ;
    }
}

static uint8_t mpu6500_read_who_am_i()
{
    uint8_t id = mpu6500_read_byte(MPU6500_WHO_AM_I);
    __msleep(5);
    return id;
}

static void mpu6500_reset()
{
    mpu6500_write_byte(MPU6500_PWR_MGMT_1, 0x80);
    __msleep(100);
}

static void mpu6500_convert_to_scale(void)
{
    mpu6500.accel_raw[0] = mpu6500.accel_unscaled[0] * mpu6500.accel_scale;
    mpu6500.accel_raw[1] = mpu6500.accel_unscaled[1] * mpu6500.accel_scale;
    mpu6500.accel_raw[2] = mpu6500.accel_unscaled[2] * mpu6500.accel_scale;
    mpu6500.gyro_raw[0] = mpu6500.gyro_unscaled[0] * mpu6500.gyro_scale;
    mpu6500.gyro_raw[1] = mpu6500.gyro_unscaled[1] * mpu6500.gyro_scale;
    mpu6500.gyro_raw[2] = mpu6500.gyro_unscaled[2] * mpu6500.gyro_scale;
    mpu6500.temp_raw = mpu6500.temp_unscaled * MPU6500T_85degC + 21.0f;
}

void mpu6500_init(void)
{
    mpu6500_interrupt_init();
    mpu6500_spi_init();
    __msleep(50);

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
    __msleep(100);

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
    __msleep(100);

    /* Gyroscope update rate = 1KHz, Low-pass filter bandwitdh = 20Hz */
    mpu6500_write_byte(MPU6500_CONFIG, GYRO_DLPF_BANDWIDTH_20Hz);
    __msleep(100);

    /* Acceleromter update rate = 1KHz, Low-pass filter bandwitdh = 20Hz */
    mpu6500_write_byte(MPU6500_ACCEL_CONFIG2, ACCEL_DLPF_BANDWIDTH_20Hz);
    __msleep(100);

    /* Enable data-ready interrupt */
    mpu6500_write_byte(MPU6500_INT_ENABLE, 0x01);
    __msleep(100);

    /* Sampling time = 0.001s (1KHz), Cutoff frequency = 25Hz */
    lpf_first_order_init(&mpu6500_lpf_gain, 0.001, 25);

    register_chrdev("accel0", &mpu6500_accel_fops);
    register_chrdev("gyro0", &mpu6500_gyro_fops);
    printk("accel0: mp6500 accelerometer");
    printk("gyro0: mpu6500 gyroscope");
}

void mpu6500_interrupt_handler(void)
{
    uint8_t buffer[14];

    /* Read measurements */
    mpu6500_spi_set_chipselect(true);
    mpu6500_spi_w8r8(MPU6500_ACCEL_XOUT_H | 0x80);
    buffer[0] = mpu6500_spi_w8r8(0xff);
    buffer[1] = mpu6500_spi_w8r8(0xff);
    buffer[2] = mpu6500_spi_w8r8(0xff);
    buffer[3] = mpu6500_spi_w8r8(0xff);
    buffer[4] = mpu6500_spi_w8r8(0xff);
    buffer[5] = mpu6500_spi_w8r8(0xff);
    buffer[6] = mpu6500_spi_w8r8(0xff);
    buffer[7] = mpu6500_spi_w8r8(0xff);
    buffer[8] = mpu6500_spi_w8r8(0xff);
    buffer[9] = mpu6500_spi_w8r8(0xff);
    buffer[10] = mpu6500_spi_w8r8(0xff);
    buffer[11] = mpu6500_spi_w8r8(0xff);
    buffer[12] = mpu6500_spi_w8r8(0xff);
    buffer[13] = mpu6500_spi_w8r8(0xff);
    mpu6500_spi_set_chipselect(false);

    /* Composite measurements */
    mpu6500.accel_unscaled[0] = -s8_to_s16(buffer[0], buffer[1]);
    mpu6500.accel_unscaled[1] = +s8_to_s16(buffer[2], buffer[3]);
    mpu6500.accel_unscaled[2] = -s8_to_s16(buffer[4], buffer[5]);
    mpu6500.temp_unscaled = s8_to_s16(buffer[6], buffer[7]);
    mpu6500.gyro_unscaled[0] = -s8_to_s16(buffer[8], buffer[9]);
    mpu6500.gyro_unscaled[1] = +s8_to_s16(buffer[10], buffer[11]);
    mpu6500.gyro_unscaled[2] = -s8_to_s16(buffer[12], buffer[13]);

    /* Convert measurementis to metric unit */
    mpu6500_convert_to_scale();

    /* Low-pass filtering for accelerometer to cancel vibration noise */
    lpf_first_order(mpu6500.accel_raw[0], &(mpu6500.accel_lpf[0]),
                    mpu6500_lpf_gain);
    lpf_first_order(mpu6500.accel_raw[1], &(mpu6500.accel_lpf[1]),
                    mpu6500_lpf_gain);
    lpf_first_order(mpu6500.accel_raw[2], &(mpu6500.accel_lpf[2]),
                    mpu6500_lpf_gain);
}

void EXTI15_10_IRQHandler(void)
{
    if (EXTI_GetITStatus(EXTI_Line10) == SET) {
        mpu6500_interrupt_handler();
        EXTI_ClearITPendingBit(EXTI_Line10);
    }
}
