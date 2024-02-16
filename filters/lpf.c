#include <math.h>

#include "lpf.h"

void lpf_first_order_init(float *ret_gain,
                          float sampling_time,
                          float cutoff_freq)
{
    /* Reference: Low-pass Filter (Wikipedia) */

    /* Return the alpha value of the first order low-pass filter */
    *ret_gain = sampling_time / (sampling_time + 1 / (2 * M_PI * cutoff_freq));
}

void lpf_first_order(float new, float *filtered, float alpha)
{
    *filtered = (new *alpha) + (*filtered * (1.0f - alpha));
}

void lpf_second_order_init(lpf2_t *lpf, float sampling_freq, float cutoff_freq)
{
    /* Reference: How does a low-pass filter programmatically work? */

    float alpha = tan(M_PI * cutoff_freq / sampling_freq);
    float sqrt2 = sqrt(2.0f);
    float alpha_squared = alpha * alpha;
    float sqrt2_alpha = sqrt2 * alpha;

    /* Initialize filter states */
    lpf->filter_last = 0.0f;
    lpf->filter_last_last = 0.0f;
    lpf->input_last = 0.0f;
    lpf->input_last_last = 0.0f;

    /* Calculate gain coefficients */
    lpf->k = alpha_squared / (1.0f + sqrt2_alpha + alpha_squared);
    lpf->a1 =
        (2.0f * (alpha_squared - 1.0f)) / (1.0f + sqrt2_alpha + alpha_squared);
    lpf->a2 = (1.0f - sqrt2_alpha + alpha_squared) /
              (1.0f + sqrt2_alpha + alpha_squared);
    lpf->b1 = 2.0f;
    lpf->b2 = 1.0f;
}

void lpf_second_order(float new_input, float *filtered_data, lpf2_t *lpf)
{
    /* Reference: How does a low-pass filter programmatically work? */

    float result = (lpf->k * new_input) + (lpf->k * lpf->b1 * lpf->input_last) +
                   (lpf->k * lpf->b2 * lpf->input_last_last) -
                   (lpf->a1 * lpf->filter_last) -
                   (lpf->a2 * lpf->filter_last_last);

    lpf->filter_last_last = lpf->filter_last;
    lpf->filter_last = result;
    lpf->input_last_last = lpf->input_last;
    lpf->input_last = new_input;

    *filtered_data = result;
}
