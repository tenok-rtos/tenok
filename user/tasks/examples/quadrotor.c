#include <fcntl.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <task.h>
#include <tenok.h>
#include <unistd.h>

#include "madgwick_filter.h"

#define deg_to_rad(angle) (angle * M_PI / 180.0)
#define rad_to_deg(radian) (radian * 180.0 / M_PI)

madgwick_t madgwick_ahrs;

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

void flight_control_task(void)
{
    setprogname("flight control");

    madgwick_init(&madgwick_ahrs, 400, 0.13);

    int accel_fd = open("/dev/accel0", 0);
    int gyro_fd = open("/dev/gyro0", 0);

    if (accel_fd || gyro_fd) {
        printf("failed to open the IMU.\n\r");
        exit(1);
    }

    float rpy[3];
    float accel[3], gravity[3];
    float gyro[3], gyro_rad[3];

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

        /* Run orientation estimation */
        madgwick_imu_ahrs(&madgwick_ahrs, gravity, gyro_rad);
        quat_to_euler(madgwick_ahrs.q, rpy);

        usleep(2500);
    }
}

HOOK_USER_TASK(flight_control_task, 3 /*THREAD_PRIORITY_MAX*/, 2048);
