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
#include "debug_link_pid_msg.h"
#include "madgwick_filter.h"
#include "pwm.h"
#include "sbus.h"

#define LED_R LED0
#define LED_G LED1
#define LED_B LED2
#define FREQ_TESTER GPIO_PIN0

#define deg_to_rad(angle) ((angle) *M_PI / 180.0)
#define rad_to_deg(radian) ((radian) *180.0 / M_PI)

/* Gyroscope bias calibration */
#define ENABLE_GYRO_CALIB
#define GYRO_SAMPLE_CNT 1000

/* PWM +width time for zero-thrust and maximum thrust */
#define THRUST_PWM_MIN 1090  // 1.09 ms
#define THRUST_PWM_MAX 2075  // 2.075 ms
#define THRUST_PWM_DIFF (THRUST_PWM_MAX - THRUST_PWM_MIN)

#define FLIGHT_CTRL_FREQ 400                             // Hz
#define FLIGHT_CTRL_PERIOD (1000.0f / FLIGHT_CTRL_FREQ)  // Millisecond

/* 10x faster than the control loop frequency we want for
 * enough precision. Note that the unit is microsecond.
 * sleep_time = (1 / freq * 1000) / 10
 */
#define FLIGHT_CTRL_SLEEP_TIME (100000 / FLIGHT_CTRL_FREQ)

typedef struct {
    float kp;
    float ki;
    float kd;
    float p_final;
    float i_final;
    float d_final;
    float setpoint;
    float error_current;
    float error_last;
    float error_integral;
    float error_derivative;
    float feedforward;
    float output;
    float output_max;
    float output_min;
    bool enable;
} pid_control_t;

static madgwick_t madgwick_ahrs;
static float rpy[3];

static pid_control_t pid_roll = {
    .kp = 0.015f,
    .kd = 0.003f,
    .output_max = +1.0f,  //+100%
    .output_min = -1.0f,  //-100%
    .enable = true,
};

static pid_control_t pid_pitch = {
    .kp = 0.015f,
    .kd = 0.003f,
    .output_max = +1.0f,
    .output_min = -1.0f,
    .enable = true,
};

static pid_control_t pid_yaw_rate = {
    .kp = 0,
    .output_max = +1.0f,
    .output_min = -1.0f,
    .enable = true,
};

static float motors[4] = {0.0f};

float calc_elapsed_time(struct timespec *tp_now, struct timespec *tp_last)
{
    return (float) (tp_now->tv_sec - tp_last->tv_sec) * 1e3 +
           (float) (tp_now->tv_nsec - tp_last->tv_nsec) * 1e-6;
}

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

void bound_float(float *val, float max, float min)
{
    if (*val > max)
        *val = max;
    else if (*val < min)
        *val = min;
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

static void rc_safety_protection(int rc_fd, int led_fd)
{
    sbus_t rc;

    struct timespec time_last, time_now;
    clock_gettime(CLOCK_MONOTONIC, &time_last);

    int blink = LED_DISABLE;
    ioctl(led_fd, LED_R, LED_DISABLE);
    ioctl(led_fd, LED_G, LED_DISABLE);
    ioctl(led_fd, LED_B, LED_DISABLE);

    /* Block and blink red LED until RC reset */
    do {
        /* Blink control */
        clock_gettime(CLOCK_MONOTONIC, &time_now);
        if (calc_elapsed_time(&time_now, &time_last) > 100.0f) {
            time_last = time_now;
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

static void attitude_pd_control(pid_control_t *pid,
                                float attitude,
                                float setpoint_attitude,
                                float angular_vel)
{
    if (!pid->enable)
        return;

    /* error = setpoint - measurement */
    pid->error_current = setpoint_attitude - attitude;
    /* error_derivative = 0 (setpoint) - measurement_derivative */
    pid->error_derivative = -angular_vel;
    pid->p_final = pid->kp * pid->error_current;
    pid->d_final = pid->kd * pid->error_derivative;
    pid->output = pid->p_final + pid->d_final;
    bound_float(&pid->output, pid->output_max, pid->output_min);
}

static void yaw_rate_p_control(pid_control_t *pid,
                               float setpoint_yaw_rate,
                               float yaw_rate)
{
    if (!pid->enable)
        return;

    /* error = setpoint - measurement */
    pid->error_current = setpoint_yaw_rate - yaw_rate;
    pid->p_final = pid->kp * pid->error_current;
    pid->output = pid->p_final;
    bound_float(&pid->output, pid->output_max, pid->output_min);
}

static void quadrotor_thrust_allocation(float throttle,
                                        float roll_ctrl,
                                        float pitch_ctrl,
                                        float yaw_ctrl,
                                        float motors[4])
{
    /*   Airframe:
       (CCW)    (CW)
        M2       M1
             X
        M3       M4
       (CW)    (CCW) */

    /* Range for throttle, roll, pitch, and yaw control all take [0, 1] */
    motors[0] = throttle - roll_ctrl + pitch_ctrl - yaw_ctrl;
    motors[1] = throttle + roll_ctrl + pitch_ctrl + yaw_ctrl;
    motors[2] = throttle + roll_ctrl - pitch_ctrl - yaw_ctrl;
    motors[3] = throttle - roll_ctrl - pitch_ctrl + yaw_ctrl;

    /* Convert [0, 1] range to [PWM_MIN, PWM_MAX] */
    motors[0] = THRUST_PWM_MIN + motors[0] * THRUST_PWM_DIFF;
    motors[1] = THRUST_PWM_MIN + motors[1] * THRUST_PWM_DIFF;
    motors[2] = THRUST_PWM_MIN + motors[2] * THRUST_PWM_DIFF;
    motors[3] = THRUST_PWM_MIN + motors[3] * THRUST_PWM_DIFF;

    /* Control signal saturation */
    bound_float(&motors[0], THRUST_PWM_MAX, THRUST_PWM_MIN);
    bound_float(&motors[1], THRUST_PWM_MAX, THRUST_PWM_MIN);
    bound_float(&motors[2], THRUST_PWM_MAX, THRUST_PWM_MIN);
    bound_float(&motors[3], THRUST_PWM_MAX, THRUST_PWM_MIN);
}

/* Place quadrotor statically before calibration */
static void gyro_bias_estimate(int gyro_fd, float gyro_bias[3])
{
    gyro_bias[0] = 0.0f;
    gyro_bias[1] = 0.0f;
    gyro_bias[2] = 0.0f;

#ifdef ENABLE_GYRO_CALIB
    float gyro[3];

    for (int i = 0; i < GYRO_SAMPLE_CNT; i++) {
        read(gyro_fd, gyro, sizeof(float[3]));

        gyro_bias[0] += gyro[0] / GYRO_SAMPLE_CNT;
        gyro_bias[1] += gyro[1] / GYRO_SAMPLE_CNT;
        gyro_bias[2] += gyro[2] / GYRO_SAMPLE_CNT;

        usleep(1000);
    }
#endif
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

    /* Open frequency test pin */
    int freq_tester_fd = open("/dev/freq_tester", 0);
    if (freq_tester_fd < 0) {
        printf("failed to open the frequency tester.\n\r");
        exit(1);
    }

    /* Open Inertial Measurement Unit (IMU) */
    int accel_fd = open("/dev/accel0", 0);
    int gyro_fd = open("/dev/gyro0", 0);

    if (accel_fd < 0 || gyro_fd < 0) {
        printf("failed to open the IMU.\n\r");
        exit(1);
    }

    float accel[3], gravity[3];   //[m/s^2]
    float gyro[3], gyro_bias[3];  //[deg/s]
    float gyro_rad[3];            //[rad/s]

    /* Gyroscope bias calibration */
    gyro_bias_estimate(gyro_fd, gyro_bias);

    /* Open PWM interface of Electrical Speed Controllers (ESC) */
    int pwm_fd = open("/dev/pwm", 0);
    if (pwm_fd < 0) {
        printf("failed to open PWM interface.\n\r");
        exit(1);
    }

    /* Initialize thrusts for motor 1 to 4 */
    ioctl(pwm_fd, SET_PWM_CHANNEL1, THRUST_PWM_MIN);
    ioctl(pwm_fd, SET_PWM_CHANNEL2, THRUST_PWM_MIN);
    ioctl(pwm_fd, SET_PWM_CHANNEL3, THRUST_PWM_MIN);
    ioctl(pwm_fd, SET_PWM_CHANNEL4, THRUST_PWM_MIN);

    /* Open remote control receiver */
    int rc_fd = open("/dev/sbus", 0);
    if (rc_fd < 0) {
        printf("failed to open remote control receiver.\n\r");
        exit(1);
    }

    sbus_t rc;
    float throttle;

    /* Wait until RC joystick positions are reset */
    rc_safety_protection(rc_fd, led_fd);

    /* Execution time */
    struct timespec time_last, time_now;
    float time_elapsed = 0.0f;
    clock_gettime(CLOCK_MONOTONIC, &time_last);

    while (1) {
        /* Loop frequency control */
        while (time_elapsed < FLIGHT_CTRL_PERIOD) {
            usleep(FLIGHT_CTRL_SLEEP_TIME);

            clock_gettime(CLOCK_MONOTONIC, &time_now);
            time_elapsed = calc_elapsed_time(&time_now, &time_last);
        }
        time_last = time_now;
        time_elapsed = 0;

        /* Read RC signal */
        read(rc_fd, &rc, sizeof(sbus_t));
        throttle = rc.throttle * 0.01f;  // Convert from [0, 100] to [0, 1]

        /* Read accelerometer */
        read(accel_fd, accel, sizeof(float[3]));
        gravity[0] = -accel[0];
        gravity[1] = -accel[1];
        gravity[2] = -accel[2];

        /* Read gyroscope */
        read(gyro_fd, gyro, sizeof(float[3]));
        gyro_rad[0] = deg_to_rad(gyro[0] - gyro_bias[0]);
        gyro_rad[1] = deg_to_rad(gyro[1] - gyro_bias[1]);
        gyro_rad[2] = deg_to_rad(gyro[2] - gyro_bias[2]);

        /* Attitude estimation */
        madgwick_imu_ahrs(&madgwick_ahrs, gravity, gyro_rad);
        quat_to_euler(madgwick_ahrs.q, rpy);

        /* Roll and pitch attitude control */
        attitude_pd_control(&pid_roll, rpy[0], -rc.roll, gyro[0]);
        attitude_pd_control(&pid_pitch, rpy[1], -rc.pitch, gyro[1]);

        /* Yaw rate control */
        yaw_rate_p_control(&pid_yaw_rate, -rc.yaw, gyro[2]);

        /* Thrust allocation */
        quadrotor_thrust_allocation(throttle, pid_roll.output, pid_pitch.output,
                                    pid_yaw_rate.output, motors);

        /* Check safety switch */
        if (rc.dual_switch1) {
            /* Motor disarmed */
            ioctl(led_fd, LED_R, LED_DISABLE);
            ioctl(led_fd, LED_G, LED_DISABLE);
            ioctl(led_fd, LED_B, LED_ENABLE);

            /* Disable all motors */
            ioctl(pwm_fd, SET_PWM_CHANNEL1, THRUST_PWM_MIN);
            ioctl(pwm_fd, SET_PWM_CHANNEL2, THRUST_PWM_MIN);
            ioctl(pwm_fd, SET_PWM_CHANNEL3, THRUST_PWM_MIN);
            ioctl(pwm_fd, SET_PWM_CHANNEL4, THRUST_PWM_MIN);
        } else {
            /* Motor armed */
            ioctl(led_fd, LED_R, LED_ENABLE);
            ioctl(led_fd, LED_G, LED_DISABLE);
            ioctl(led_fd, LED_B, LED_DISABLE);

            /* Set control output */
            ioctl(pwm_fd, SET_PWM_CHANNEL1, motors[0]);
            ioctl(pwm_fd, SET_PWM_CHANNEL2, motors[1]);
            ioctl(pwm_fd, SET_PWM_CHANNEL3, motors[2]);
            ioctl(pwm_fd, SET_PWM_CHANNEL4, motors[3]);
        }

        /* Frequency testing */
        ioctl(freq_tester_fd, FREQ_TESTER, GPIO_TOGGLE);
    }
}

void debug_link_task(void)
{
    setprogname("debug link");

    int debug_link_fd = open("/dev/dbglink", O_RDWR);

    debug_link_msg_attitude_t msg;
    debug_link_msg_pid_t pid_msg;
    uint8_t buf[100];
    size_t size;

    /* 10Hz */
    while (1) {
        msg.q[0] = madgwick_ahrs.q[0];
        msg.q[1] = madgwick_ahrs.q[1];
        msg.q[2] = madgwick_ahrs.q[2];
        msg.q[3] = madgwick_ahrs.q[3];
        msg.rpy[0] = rpy[0];
        msg.rpy[1] = rpy[1];
        msg.rpy[2] = rpy[2];
        size = pack_debug_link_attitude_msg(&msg, buf);
        write(debug_link_fd, buf, size);
        usleep(50000);

        pid_msg.error_rpy[0] = pid_roll.output;
        pid_msg.error_rpy[1] = pid_pitch.output;
        pid_msg.error_rpy[2] = 0.0f;
        size = pack_debug_link_pid_msg(&pid_msg, buf);
        write(debug_link_fd, buf, size);
        usleep(50000);
    }
}

HOOK_USER_TASK(flight_control_task, THREAD_PRIORITY_MAX, 2048);
HOOK_USER_TASK(debug_link_task, 3, 1024);
