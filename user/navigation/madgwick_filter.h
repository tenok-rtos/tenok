#ifndef __MADGWICK_FILTER_H__
#define __MADGWICK_FILTER_H__

#include <stdint.h>
#include "math.h"

typedef struct {
    float beta;
    float dt;
    float q[4];
} madgwick_t;

void madgwick_init(madgwick_t *madgwick, float sample_rate, float beta);
void madgwick_imu_ahrs(madgwick_t *madgwick, float *accel, float *gyro);
void madgwick_margs_ahrs(madgwick_t *madgwick,
                         float *accel,
                         float *gyro,
                         float *mag);

#endif
