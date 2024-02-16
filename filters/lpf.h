#ifndef __LPF_H__
#define __LPF_H__

/* Second order low-pass filter */
typedef struct {
    float k;
    float a1;
    float a2;
    float b1;
    float b2;

    float filter_last;
    float filter_last_last;

    float input_last;
    float input_last_last;
} lpf2_t;

void lpf_first_order_init(float *ret_gain,
                          float sampling_time,
                          float cutoff_freq);
void lpf_first_order(float new, float *filtered, float alpha);

void lpf_second_order_init(lpf2_t *lpf,
                           float sampling_freq,
                           float cuttoff_freq);
void lpf_second_order(float new_input, float *filtered_data, lpf2_t *lpf);

#endif
