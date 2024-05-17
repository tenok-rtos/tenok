#include <fcntl.h>
#include <ioctl.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <task.h>
#include <tenok.h>
#include <unistd.h>

#include "bsp_drv.h"
#include "debug_link_attitude_msg.h"
#include "madgwick_filter.h"
#include "pwm.h"
#include "sbus.h"

#define LED_R LED0
#define LED_G LED1
#define LED_B LED2

#define deg_to_rad(angle) (angle * M_PI / 180.0)
#define rad_to_deg(radian) (radian * 180.0 / M_PI)

/* Zero-thrust PWM +width time */
#define ZERO_THRUST_PWM_WIDTH 1090  // 1.09ms

static madgwick_t madgwick_ahrs;
static float rpy[3];

void quat_to_euler(float *q, float *rpy)
{
    rpy[0] = atan2(2.0 * (q[0] * q[1] + q[2] * q[3]),
                   1.0 - 2.0 * (q[1] * q[1] + q[2] * q[2]));
    rpy[1] = asin(2.0 * (q[0] * q[2] - q[3] * q[1]));
    rpy[2] = atan2(2.0 * (q[0] * q[3] + q[1] * q[2]),
                   1.0 - 2.0 * (q[2] * q[2] + q[3] * q[3]));

    rpy[0] = rad_to_deg(rpy[0]);
    rpy[1] = rad_to_deg(rpy[1]);
    rpy[2] = rad_to_deg(rpy[2]);
}

static int rc_safety_check(sbus_t *rc)
{
    if (rc->dual_switch1 == false)
        return 1;
    if (rc->throttle > 10.0f)
        return 1;
    if (rc->roll > 5.0f || rc->roll < -5.0f)
        return 1;
    if (rc->pitch > 5.0f || rc->pitch < -5.0f)
        return 1;
    if (rc->yaw > 5.0f || rc->yaw < -5.0f)
        return 1;

    return 0;
}

float get_sys_time_ms(void)
{
    struct timespec tp;
    clock_gettime(CLOCK_MONOTONIC, &tp);

    return ((float) tp.tv_sec * 1e3) + (float) (tp.tv_nsec * 1e-6);
}

void rc_safety_protection(int rc_fd, int led_fd)
{
    sbus_t rc;

    float time_last = 0;
    float time_current = 0;

    int blink = LED_DISABLE;
    ioctl(led_fd, LED_R, LED_DISABLE);
    ioctl(led_fd, LED_G, LED_DISABLE);
    ioctl(led_fd, LED_B, LED_DISABLE);

    /* Block and blink red LED until RC reset */
    do {
        /* Blink control */
        time_current = get_sys_time_ms();

        if (time_current - time_last > 100.0f) {
            time_last = time_current;
            blink = (blink + 1) % 2;
            ioctl(led_fd, LED_R, blink);
        }

        /* Update RC signal */
        read(rc_fd, &rc, sizeof(sbus_t));

        /* Yield task for a while */
        usleep(1000);
    } while (rc_safety_check(&rc));

    /* Light blue LED */
    ioctl(led_fd, LED_R, LED_DISABLE);
    ioctl(led_fd, LED_G, LED_DISABLE);
    ioctl(led_fd, LED_B, LED_ENABLE);
}

void flight_control_task(void)
{
    setprogname("flight control");

    /* Initialize Madgwick Filter for attitude estimation */
    madgwick_init(&madgwick_ahrs, 400, 0.13);

    /* Open RGB LED */
    int led_fd = open("/dev/led", 0);
    if (led_fd < 0) {
        printf("failed to open the RGB LED.\n\r");
        exit(1);
    }

    /* Open Inertial Measurement Unit (IMU) */
    int accel_fd = open("/dev/accel0", 0);
    int gyro_fd = open("/dev/gyro0", 0);

    if (accel_fd < 0 || gyro_fd < 0) {
        printf("failed to open the IMU.\n\r");
        exit(1);
    }

    float accel[3], gravity[3];
    float gyro[3], gyro_rad[3];

    /* Open PWM interface of Electrical Speed Controllers (ESC) */
    int pwm_fd = open("/dev/pwm", 0);
    if (pwm_fd < 0) {
        printf("failed to open PWM interface.\n\r");
        exit(1);
    }

    /* Initialize thrusts for motor 1 to 4 */
    ioctl(pwm_fd, SET_PWM_CHANNEL1, ZERO_THRUST_PWM_WIDTH);
    ioctl(pwm_fd, SET_PWM_CHANNEL2, ZERO_THRUST_PWM_WIDTH);
    ioctl(pwm_fd, SET_PWM_CHANNEL3, ZERO_THRUST_PWM_WIDTH);
    ioctl(pwm_fd, SET_PWM_CHANNEL4, ZERO_THRUST_PWM_WIDTH);

    /* Open remote control receiver */
    int rc_fd = open("/dev/sbus", 0);
    if (rc_fd < 0) {
        printf("failed to open remote control receiver.\n\r");
        exit(1);
    }

    /* Wait until RC joystick positions are reset */
    rc_safety_protection(rc_fd, led_fd);

    while (1) {
        /* Read accelerometer */
        read(accel_fd, accel, sizeof(float[3]));
        gravity[0] = -accel[0];
        gravity[1] = -accel[1];
        gravity[2] = -accel[2];

        /* Read gyroscope */
        read(gyro_fd, gyro, sizeof(float[3]));
        gyro_rad[0] = deg_to_rad(gyro[0]);
        gyro_rad[1] = deg_to_rad(gyro[1]);
        gyro_rad[2] = deg_to_rad(gyro[2]);

        /* Run attitude estimation */
        madgwick_imu_ahrs(&madgwick_ahrs, gravity, gyro_rad);
        quat_to_euler(madgwick_ahrs.q, rpy);

        usleep(2500);
    }
}

void debug_link_task(void)
{
    setprogname("debug link");

    int debug_link_fd = open("/dev/dbglink", O_RDWR);

    debug_link_msg_attitude_t msg;
    uint8_t buf[100];

    while (1) {
        size_t size = pack_debug_link_attitude_msg(&msg, buf);
        write(debug_link_fd, buf, size);

        msg.rpy[0] = rpy[0];
        msg.rpy[1] = rpy[1];
        msg.rpy[2] = rpy[2];

        usleep(10000); /* 100Hz (10ms) */
    }
}

HOOK_USER_TASK(flight_control_task, 3 /*THREAD_PRIORITY_MAX*/, 2048);
HOOK_USER_TASK(debug_link_task, 3, 1024);
