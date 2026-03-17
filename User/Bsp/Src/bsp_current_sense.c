/**
 * @file    bsp_current_sense.c
 * @brief   Three-phase current sensing implementation.
 */

#include "bsp_current_sense.h"
#include "bsp_adc_motor.h"

#include <math.h>
#include <string.h>

#define BSP_CURRENT_SENSE_SAMPLE_TIMEOUT_MS   (50U)

static bsp_current_calib_t s_calib = {
    .u_offset = BSP_CURRENT_SENSE_ADC_ZERO,
    .v_offset = BSP_CURRENT_SENSE_ADC_ZERO,
    .w_offset = BSP_CURRENT_SENSE_ADC_ZERO,
    .calibrated = false
};

static bsp_current_t s_current_raw = {0};
static bsp_current_t s_current_filtered = {0};
static bool s_overcurrent_flag = false;

static inline void first_order_lpf(float *yn_1, float xn, float alpha)
{
    *yn_1 = (1.0f - alpha) * (*yn_1) + alpha * xn;
}

uint8_t bsp_current_sense_init(void)
{
    memset(&s_current_raw, 0, sizeof(s_current_raw));
    memset(&s_current_filtered, 0, sizeof(s_current_filtered));
    s_overcurrent_flag = false;

    return bsp_current_sense_calibrate(200U);
}

uint8_t bsp_current_sense_calibrate(uint16_t samples)
{
    uint32_t sum_u = 0U;
    uint32_t sum_v = 0U;
    uint32_t sum_w = 0U;
    uint32_t last_update;
    uint32_t start_tick;
    uint32_t current_update;
    uint16_t i;
    bool sample_ready;

    s_calib.calibrated = false;

    if (samples == 0U) {
        return 1U;
    }

    last_update = bsp_adc_get_injected_update_count();

    for (i = 0U; i < samples; i++) {
        start_tick = HAL_GetTick();
        sample_ready = false;

        while ((HAL_GetTick() - start_tick) < BSP_CURRENT_SENSE_SAMPLE_TIMEOUT_MS) {
            current_update = bsp_adc_get_injected_update_count();

            if (current_update != last_update) {
                last_update = current_update;
                sample_ready = true;
                break;
            }

            __NOP();
        }

        if (!sample_ready) {
            return 2U;
        }

        sum_u += bsp_adc_get_phase_u_raw();
        sum_v += bsp_adc_get_phase_v_raw();
        sum_w += bsp_adc_get_phase_w_raw();
    }

    s_calib.u_offset = (uint16_t)(sum_u / samples);
    s_calib.v_offset = (uint16_t)(sum_v / samples);
    s_calib.w_offset = (uint16_t)(sum_w / samples);
    s_calib.calibrated = true;

    return 0U;
}

bool bsp_current_sense_is_calibrated(void)
{
    return s_calib.calibrated;
}

void bsp_current_sense_get_calib(bsp_current_calib_t *calib)
{
    if (calib != NULL) {
        *calib = s_calib;
    }
}

void bsp_current_sense_set_calib(const bsp_current_calib_t *calib)
{
    if (calib != NULL) {
        s_calib = *calib;
    }
}

float bsp_current_sense_adc_to_ma(uint16_t adc_raw, uint16_t offset)
{
    int32_t adc_diff = (int32_t)adc_raw - (int32_t)offset;
    return (float)adc_diff * BSP_CURRENT_SENSE_LSB_TO_MA;
}

void bsp_current_sense_get_raw(bsp_current_t *current)
{
    uint16_t u_raw;
    uint16_t v_raw;
    uint16_t w_raw;

    if (current == NULL) {
        return;
    }

    u_raw = bsp_adc_get_phase_u_raw();
    v_raw = bsp_adc_get_phase_v_raw();
    w_raw = bsp_adc_get_phase_w_raw();

    current->u_ma = bsp_current_sense_adc_to_ma(u_raw, s_calib.u_offset);
    current->v_ma = bsp_current_sense_adc_to_ma(v_raw, s_calib.v_offset);
    current->w_ma = bsp_current_sense_adc_to_ma(w_raw, s_calib.w_offset);
    current->bus_ma = current->u_ma + current->v_ma + current->w_ma;

    s_current_raw = *current;
}

void bsp_current_sense_get_filtered(bsp_current_t *current)
{
    if (current == NULL) {
        return;
    }

    *current = s_current_filtered;
}

void bsp_current_sense_update_filter(void)
{
    float max_current;

    bsp_current_sense_get_raw(&s_current_raw);

    first_order_lpf(&s_current_filtered.u_ma, s_current_raw.u_ma, BSP_CURRENT_SENSE_LPF_ALPHA);
    first_order_lpf(&s_current_filtered.v_ma, s_current_raw.v_ma, BSP_CURRENT_SENSE_LPF_ALPHA);
    first_order_lpf(&s_current_filtered.w_ma, s_current_raw.w_ma, BSP_CURRENT_SENSE_LPF_ALPHA);
    first_order_lpf(&s_current_filtered.bus_ma, s_current_raw.bus_ma, BSP_CURRENT_SENSE_LPF_ALPHA);

    max_current = bsp_current_sense_get_max_phase_current();
    s_overcurrent_flag = (max_current > BSP_CURRENT_SENSE_OC_THRESHOLD_MA);
}

bool bsp_current_sense_is_overcurrent(void)
{
    return s_overcurrent_flag;
}

float bsp_current_sense_get_max_phase_current(void)
{
    float abs_u = fabsf(s_current_filtered.u_ma);
    float abs_v = fabsf(s_current_filtered.v_ma);
    float abs_w = fabsf(s_current_filtered.w_ma);
    float max = abs_u;

    if (abs_v > max) {
        max = abs_v;
    }
    if (abs_w > max) {
        max = abs_w;
    }

    return max;
}
