#include "madgwick_filter.h"
#include "arm_math.h"

/* Reference paper: "An efficient orientation filter for inertial and
   inertial/magnetic sensor arrays," Sebastian O.H. Madgwick */

void madgwick_init(madgwick_t *madgwick, float sample_rate, float beta)
{
    madgwick->beta = beta;
    madgwick->dt = 1.0f / sample_rate;
}

void madgwick_imu_ahrs(madgwick_t *madgwick, float *accel, float *gyro)
{
    float q0_dot = 0.5f * (-madgwick->q[1] * gyro[0] -
                           madgwick->q[2] * gyro[1] - madgwick->q[3] * gyro[2]);
    float q1_dot = 0.5f * (madgwick->q[0] * gyro[0] + madgwick->q[2] * gyro[2] -
                           madgwick->q[3] * gyro[1]);
    float q2_dot = 0.5f * (madgwick->q[0] * gyro[1] - madgwick->q[1] * gyro[2] +
                           madgwick->q[3] * gyro[0]);
    float q3_dot = 0.5f * (madgwick->q[0] * gyro[2] + madgwick->q[1] * gyro[1] -
                           madgwick->q[2] * gyro[0]);

    float accel_norm;
    arm_sqrt_f32(
        accel[0] * accel[0] + accel[1] * accel[1] + accel[2] * accel[2],
        &accel_norm);
    accel_norm = 1.0f / accel_norm;
    accel[0] *= accel_norm;
    accel[1] *= accel_norm;
    accel[2] *= accel_norm;

    float _2q0 = 2.0f * madgwick->q[0];
    float _2q1 = 2.0f * madgwick->q[1];
    float _2q2 = 2.0f * madgwick->q[2];
    float _2q3 = 2.0f * madgwick->q[3];
    float _4q0 = 4.0f * madgwick->q[0];
    float _4q1 = 4.0f * madgwick->q[1];
    float _4q2 = 4.0f * madgwick->q[2];
    float _4q3 = 4.0f * madgwick->q[3];
    float q0q0 = madgwick->q[0] * madgwick->q[0];
    float q1q1 = madgwick->q[1] * madgwick->q[1];
    float q2q2 = madgwick->q[2] * madgwick->q[2];
    float q3q3 = madgwick->q[3] * madgwick->q[3];
    float q1q1_q2q2 = q1q1 + q2q2;

    /* Gradient decent algorithm corrective step */
    float g0 = _4q0 * q1q1_q2q2 + _2q2 * accel[0] - _2q1 * accel[1];
    float g1 =
        _4q1 * q1q1_q2q2 - _2q3 * accel[0] - _2q0 * accel[1] + _4q1 * accel[2];
    float g2 =
        _4q2 * q1q1_q2q2 + _2q0 * accel[0] - _2q3 * accel[1] + _4q2 * accel[2];
    float g3 = _4q3 * q1q1_q2q2 - _2q1 * accel[0] - _2q2 * accel[1];

    /* Normalize step magnitude */
    float g_norm;
    arm_sqrt_f32(g0 * g0 + g1 * g1 + g2 * g2 + g3 * g3, &g_norm);
    g_norm = 1.0f / g_norm;
    g0 *= g_norm;
    g1 *= g_norm;
    g2 *= g_norm;
    g3 *= g_norm;

    q0_dot -= madgwick->beta * g0;
    q1_dot -= madgwick->beta * g1;
    q2_dot -= madgwick->beta * g2;
    q3_dot -= madgwick->beta * g3;

    madgwick->q[0] += q0_dot * madgwick->dt;
    madgwick->q[1] += q1_dot * madgwick->dt;
    madgwick->q[2] += q2_dot * madgwick->dt;
    madgwick->q[3] += q3_dot * madgwick->dt;

    float q_norm;
    arm_sqrt_f32(q0q0 + q1q1 + q2q2 + q3q3, &q_norm);
    q_norm = 1.0f / q_norm;
    madgwick->q[0] *= q_norm;
    madgwick->q[1] *= q_norm;
    madgwick->q[2] *= q_norm;
    madgwick->q[3] *= q_norm;
}

void madgwick_margs_ahrs(madgwick_t *madgwick,
                         float *accel,
                         float *gyro,
                         float *mag)
{
    if ((mag[0] == 0.0f) && (mag[1] == 0.0f) && (mag[2] == 0.0f)) {
        madgwick_imu_ahrs(madgwick, accel, gyro);
        return;
    }

    float q0_dot = 0.5f * (-madgwick->q[1] * gyro[0] -
                           madgwick->q[2] * gyro[1] - madgwick->q[3] * gyro[2]);
    float q1_dot = 0.5f * (madgwick->q[0] * gyro[0] + madgwick->q[2] * gyro[2] -
                           madgwick->q[3] * gyro[1]);
    float q2_dot = 0.5f * (madgwick->q[0] * gyro[1] - madgwick->q[1] * gyro[2] +
                           madgwick->q[3] * gyro[0]);
    float q3_dot = 0.5f * (madgwick->q[0] * gyro[2] + madgwick->q[1] * gyro[1] -
                           madgwick->q[2] * gyro[0]);

    float accel_norm;
    arm_sqrt_f32(
        accel[0] * accel[0] + accel[1] * accel[1] + accel[2] * accel[2],
        &accel_norm);
    accel_norm = 1.0f / accel_norm;
    accel[0] *= accel_norm;
    accel[1] *= accel_norm;
    accel[2] *= accel_norm;

    float mag_norm;
    arm_sqrt_f32(mag[0] * mag[0] + mag[1] * mag[1] + mag[2] * mag[2],
                 &mag_norm);
    mag_norm = 1.0f / mag_norm;
    mag[0] *= mag_norm;
    mag[1] *= mag_norm;
    mag[2] *= mag_norm;

    float _2q0mx = 2.0f * madgwick->q[0] * mag[0];
    float _2q0my = 2.0f * madgwick->q[0] * mag[1];
    float _2q0mz = 2.0f * madgwick->q[0] * mag[2];
    float _2q1mx = 2.0f * madgwick->q[1] * mag[0];
    float _2q0 = 2.0f * madgwick->q[0];
    float _2q1 = 2.0f * madgwick->q[1];
    float _2q2 = 2.0f * madgwick->q[2];
    float _2q3 = 2.0f * madgwick->q[3];
    float _4q0 = 4.0f * madgwick->q[0];
    float _4q1 = 4.0f * madgwick->q[1];
    float _4q2 = 4.0f * madgwick->q[2];
    float _4q3 = 4.0f * madgwick->q[3];
    float q0q0 = madgwick->q[0] * madgwick->q[0];
    float q0q1 = madgwick->q[0] * madgwick->q[1];
    float q0q2 = madgwick->q[0] * madgwick->q[2];
    float q0q3 = madgwick->q[0] * madgwick->q[3];
    float q1q1 = madgwick->q[1] * madgwick->q[1];
    float q1q2 = madgwick->q[1] * madgwick->q[2];
    float q1q3 = madgwick->q[1] * madgwick->q[3];
    float q2q2 = madgwick->q[2] * madgwick->q[2];
    float q2q3 = madgwick->q[2] * madgwick->q[3];
    float q3q3 = madgwick->q[3] * madgwick->q[3];
    float q1q1_q2q2 = q1q1 + q2q2;

    /* Reference direction of earth's magnetic field */
    float hx = mag[0] * q0q0 - _2q0my * madgwick->q[3] +
               _2q0mz * madgwick->q[2] + mag[0] * q1q1 +
               _2q1 * mag[1] * madgwick->q[2] + _2q1 * mag[2] * madgwick->q[3] -
               mag[0] * q2q2 - mag[0] * q3q3;
    float hy = _2q0mx * madgwick->q[3] + mag[1] * q0q0 -
               _2q0mz * madgwick->q[1] + _2q1mx * madgwick->q[2] -
               mag[1] * q1q1 + mag[1] * q2q2 + _2q2 * mag[2] * madgwick->q[3] -
               mag[1] * q3q3;
    float _2bx;
    arm_sqrt_f32(hx * hx + hy * hy, &_2bx);
    float _2bz = -_2q0mx * madgwick->q[2] + _2q0my * madgwick->q[1] +
                 mag[2] * q0q0 + _2q1mx * madgwick->q[3] - mag[2] * q1q1 +
                 _2q2 * mag[1] * madgwick->q[3] - mag[2] * q2q2 + mag[2] * q3q3;
    float _4bx = 2.0f * _2bx;
    float _4bz = 2.0f * _2bz;

    /* Gradient decent algorithm corrective step */
    float g0 =
        _4q0 * q1q1_q2q2 + _2q2 * accel[0] - _2q1 * accel[1] -
        _2bz * madgwick->q[2] *
            (_2bx * (0.5f - q2q2 - q3q3) + _2bz * (q1q3 - q0q2) - mag[0]) +
        (-_2bx * madgwick->q[3] + _2bz * madgwick->q[1]) *
            (_2bx * (q1q2 - q0q3) + _2bz * (q0q1 + q2q3) - mag[1]) +
        _2bx * madgwick->q[2] *
            (_2bx * (q0q2 + q1q3) + _2bz * (0.5f - q1q1 - q2q2) - mag[2]);
    float g1 =
        _4q1 * q1q1_q2q2 - _2q3 * accel[0] - _2q0 * accel[1] + _4q1 * accel[2] +
        _2bz * madgwick->q[3] *
            (_2bx * (0.5f - q2q2 - q3q3) + _2bz * (q1q3 - q0q2) - mag[0]) +
        (_2bx * madgwick->q[2] + _2bz * madgwick->q[0]) *
            (_2bx * (q1q2 - q0q3) + _2bz * (q0q1 + q2q3) - mag[1]) +
        (_2bx * madgwick->q[3] - _4bz * madgwick->q[1]) *
            (_2bx * (q0q2 + q1q3) + _2bz * (0.5f - q1q1 - q2q2) - mag[2]);
    float g2 =
        _4q2 * q1q1_q2q2 + _2q0 * accel[0] - _2q3 * accel[1] + _4q2 * accel[2] +
        (-_4bx * madgwick->q[2] - _2bz * madgwick->q[0]) *
            (_2bx * (0.5f - q2q2 - q3q3) + _2bz * (q1q3 - q0q2) - mag[0]) +
        (_2bx * madgwick->q[1] + _2bz * madgwick->q[3]) *
            (_2bx * (q1q2 - q0q3) + _2bz * (q0q1 + q2q3) - mag[1]) +
        (_2bx * madgwick->q[0] - _4bz * madgwick->q[2]) *
            (_2bx * (q0q2 + q1q3) + _2bz * (0.5f - q1q1 - q2q2) - mag[2]);
    float g3 =
        _4q3 * q1q1_q2q2 - _2q1 * accel[0] - _2q2 * accel[1] +
        (-_4bx * madgwick->q[3] + _2bz * madgwick->q[1]) *
            (_2bx * (0.5f - q2q2 - q3q3) + _2bz * (q1q3 - q0q2) - mag[0]) +
        (-_2bx * madgwick->q[0] + _2bz * madgwick->q[2]) *
            (_2bx * (q1q2 - q0q3) + _2bz * (q0q1 + q2q3) - mag[1]) +
        _2bx * madgwick->q[1] *
            (_2bx * (q0q2 + q1q3) + _2bz * (0.5f - q1q1 - q2q2) - mag[2]);

    /* Normalize step magnitude */
    float g_norm;
    arm_sqrt_f32(g0 * g0 + g1 * g1 + g2 * g2 + g3 * g3, &g_norm);
    g_norm = 1.0f / g_norm;
    g0 *= g_norm;
    g1 *= g_norm;
    g2 *= g_norm;
    g3 *= g_norm;

    q0_dot -= madgwick->beta * g0;
    q1_dot -= madgwick->beta * g1;
    q2_dot -= madgwick->beta * g2;
    q3_dot -= madgwick->beta * g3;

    /* Apply feedback step */
    madgwick->q[0] += q0_dot * madgwick->dt;
    madgwick->q[1] += q1_dot * madgwick->dt;
    madgwick->q[2] += q2_dot * madgwick->dt;
    madgwick->q[3] += q3_dot * madgwick->dt;

    float q_norm;
    arm_sqrt_f32(q0q0 + q1q1 + q2q2 + q3q3, &q_norm);
    q_norm = 1.0f / q_norm;
    madgwick->q[0] *= q_norm;
    madgwick->q[1] *= q_norm;
    madgwick->q[2] *= q_norm;
    madgwick->q[3] *= q_norm;
}
