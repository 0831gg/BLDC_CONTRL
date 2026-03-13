#include "bsp_adc_motor.h"

#include "adc.h"

#include <math.h>
#include <stddef.h>

#define BSP_ADC_REF_VOLTAGE           (3.3f)
#define BSP_ADC_FULL_SCALE            (4095.0f)
#define BSP_ADC_VBUS_DIVIDER_RATIO    (25.0f)
#define BSP_ADC_TEMP_PULLUP_OHM       (4700.0f)
#define BSP_ADC_TEMP_R25_OHM          (10000.0f)
#define BSP_ADC_TEMP_BETA             (3380.0f)
#define BSP_ADC_TEMP_T0_KELVIN        (298.15f)
#define BSP_ADC_ERROR_ADC1_CAL        (1U)
#define BSP_ADC_ERROR_ADC2_CAL        (2U)
#define BSP_ADC_ERROR_ADC1_DMA_START  (3U)
#define BSP_ADC_ERROR_ADC2_INJ_START  (4U)
#define BSP_ADC_ERROR_ADC1_START      (7U)
#define BSP_ADC_ERROR_ADC1_POLL_1     (8U)
#define BSP_ADC_ERROR_ADC1_POLL_2     (9U)
#define BSP_ADC_ERROR_ADC1_STOP       (10U)

static uint16_t s_adc1_dma_buf[2] = {0U, 0U};
static volatile uint16_t s_phase_u_raw = 0U;
static volatile uint16_t s_phase_v_raw = 0U;
static volatile uint16_t s_phase_w_raw = 0U;
static volatile uint32_t s_injected_update_count = 0U;
static uint8_t s_adc1_mode = BSP_ADC1_MODE_DMA;

static uint8_t bsp_adc_regular_refresh(void)
{
    if (HAL_ADC_Start(&hadc1) != HAL_OK) {
        return BSP_ADC_ERROR_ADC1_START;
    }

    if (HAL_ADC_PollForConversion(&hadc1, 10U) != HAL_OK) {
        (void)HAL_ADC_Stop(&hadc1);
        return BSP_ADC_ERROR_ADC1_POLL_1;
    }
    s_adc1_dma_buf[0] = (uint16_t)HAL_ADC_GetValue(&hadc1);

    if (HAL_ADC_PollForConversion(&hadc1, 10U) != HAL_OK) {
        (void)HAL_ADC_Stop(&hadc1);
        return BSP_ADC_ERROR_ADC1_POLL_2;
    }
    s_adc1_dma_buf[1] = (uint16_t)HAL_ADC_GetValue(&hadc1);

    if (HAL_ADC_Stop(&hadc1) != HAL_OK) {
        return BSP_ADC_ERROR_ADC1_STOP;
    }

    return 0U;
}

uint8_t bsp_adc_motor_get_adc1_mode(void)
{
    return s_adc1_mode;
}

uint8_t bsp_adc_motor_use_polling_backup(void)
{
    uint8_t status;

    s_adc1_dma_buf[0] = 0U;
    s_adc1_dma_buf[1] = 0U;

    status = bsp_adc_regular_refresh();
    if (status == 0U) {
        s_adc1_mode = BSP_ADC1_MODE_POLLING;
    }

    return status;
}

uint8_t bsp_adc_motor_init(void)
{
    s_adc1_dma_buf[0] = 0U;
    s_adc1_dma_buf[1] = 0U;
    s_phase_u_raw = 0U;
    s_phase_v_raw = 0U;
    s_phase_w_raw = 0U;
    s_injected_update_count = 0U;
    s_adc1_mode = BSP_ADC1_MODE_DMA;

    if (HAL_ADCEx_Calibration_Start(&hadc1, ADC_SINGLE_ENDED) != HAL_OK) {
        return BSP_ADC_ERROR_ADC1_CAL;
    }

    if (HAL_ADCEx_Calibration_Start(&hadc2, ADC_SINGLE_ENDED) != HAL_OK) {
        return BSP_ADC_ERROR_ADC2_CAL;
    }

    if (HAL_ADC_Start_DMA(&hadc1, (uint32_t *)s_adc1_dma_buf, 2U) != HAL_OK) {
        uint8_t backup_status = bsp_adc_motor_use_polling_backup();
        if (backup_status != 0U) {
            return BSP_ADC_ERROR_ADC1_DMA_START;
        }
    }

    if (HAL_ADCEx_InjectedStart_IT(&hadc2) != HAL_OK) {
        return BSP_ADC_ERROR_ADC2_INJ_START;
    }

    return 0U;
}

uint16_t bsp_adc_get_vbus_raw(void)
{
    return s_adc1_dma_buf[0];
}

uint16_t bsp_adc_get_temp_raw(void)
{
    return s_adc1_dma_buf[1];
}

float bsp_adc_get_vbus(void)
{
    return ((float)bsp_adc_get_vbus_raw() / BSP_ADC_FULL_SCALE) * BSP_ADC_REF_VOLTAGE * BSP_ADC_VBUS_DIVIDER_RATIO;
}

float bsp_adc_get_temp(void)
{
    float raw = (float)bsp_adc_get_temp_raw();
    float v_adc;
    float r_ntc;

    if (raw <= 0.0f) {
        raw = 1.0f;
    }

    if (raw >= BSP_ADC_FULL_SCALE) {
        raw = BSP_ADC_FULL_SCALE - 1.0f;
    }

    v_adc = (raw / BSP_ADC_FULL_SCALE) * BSP_ADC_REF_VOLTAGE;
    r_ntc = BSP_ADC_TEMP_PULLUP_OHM * (BSP_ADC_REF_VOLTAGE - v_adc) / v_adc;

    return (1.0f / ((1.0f / BSP_ADC_TEMP_T0_KELVIN) + (logf(r_ntc / BSP_ADC_TEMP_R25_OHM) / BSP_ADC_TEMP_BETA))) - 273.15f;
}

uint16_t bsp_adc_get_phase_u_raw(void)
{
    return s_phase_u_raw;
}

uint16_t bsp_adc_get_phase_v_raw(void)
{
    return s_phase_v_raw;
}

uint16_t bsp_adc_get_phase_w_raw(void)
{
    return s_phase_w_raw;
}

void bsp_adc_get_phase_current(int16_t *iu, int16_t *iv, int16_t *iw)
{
    if (iu != NULL) {
        *iu = (int16_t)s_phase_u_raw;
    }

    if (iv != NULL) {
        *iv = (int16_t)s_phase_v_raw;
    }

    if (iw != NULL) {
        *iw = (int16_t)s_phase_w_raw;
    }
}

uint32_t bsp_adc_get_injected_update_count(void)
{
    return s_injected_update_count;
}

void HAL_ADCEx_InjectedConvCpltCallback(ADC_HandleTypeDef *hadc)
{
    if (hadc->Instance == ADC2) {
        s_phase_u_raw = (uint16_t)HAL_ADCEx_InjectedGetValue(hadc, ADC_INJECTED_RANK_1);
        s_phase_v_raw = (uint16_t)HAL_ADCEx_InjectedGetValue(hadc, ADC_INJECTED_RANK_2);
        s_phase_w_raw = (uint16_t)HAL_ADCEx_InjectedGetValue(hadc, ADC_INJECTED_RANK_3);
        s_injected_update_count++;
    }
}
