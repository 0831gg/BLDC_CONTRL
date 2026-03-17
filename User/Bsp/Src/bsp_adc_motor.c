/**
 * @file    bsp_adc_motor.c
 * @brief   Motor ADC sampling driver implementation.
 */

#include "bsp_adc_motor.h"

#include "adc.h"

#include <math.h>
#include <stddef.h>

#define BSP_ADC_REF_VOLTAGE        (3.3f)
#define BSP_ADC_FULL_SCALE         (4095.0f)
#define BSP_ADC_VBUS_DIVIDER_RATIO (25.0f)
#define BSP_ADC_VBUS_CALIBRATION   (1.0204f)

#define BSP_ADC_TEMP_PULLUP_OHM    (4700.0f)
#define BSP_ADC_TEMP_R25_OHM       (10000.0f)
#define BSP_ADC_TEMP_BETA          (3380.0f)
#define BSP_ADC_TEMP_T0_KELVIN     (298.15f)

#define BSP_ADC_ERROR_ADC1_CAL       (1U)
#define BSP_ADC_ERROR_ADC2_CAL       (2U)
#define BSP_ADC_ERROR_ADC1_DMA_START (3U)
#define BSP_ADC_ERROR_ADC2_INJ_START (4U)
#define BSP_ADC_ERROR_ADC1_START     (7U)
#define BSP_ADC_ERROR_ADC1_POLL_1    (8U)
#define BSP_ADC_ERROR_ADC1_POLL_2    (9U)
#define BSP_ADC_ERROR_ADC1_STOP      (10U)

static volatile uint16_t s_adc1_dma_buf[2] = {0U, 0U};
static volatile uint16_t s_phase_u_raw = 0U;
static volatile uint16_t s_phase_v_raw = 0U;
static volatile uint16_t s_phase_w_raw = 0U;
static volatile uint32_t s_injected_update_count = 0U;

/* [Fix2] 原子快照：主循环通过此结构读取，避免撕裂读 */
typedef struct {
    uint16_t u;
    uint16_t v;
    uint16_t w;
} phase_snapshot_t;
static volatile phase_snapshot_t s_phase_snapshot = {0U, 0U, 0U};
static uint8_t s_adc1_mode = BSP_ADC1_MODE_DMA;
static uint32_t s_adc1_polling_refresh_tick = 0U;
static uint8_t s_adc1_polling_refresh_valid = 0U;

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

static void bsp_adc_polling_refresh_if_needed(void)
{
    uint32_t now;

    if (s_adc1_mode != BSP_ADC1_MODE_POLLING) {
        return;
    }

    now = HAL_GetTick();

    if ((s_adc1_polling_refresh_valid != 0U) && (s_adc1_polling_refresh_tick == now)) {
        return;
    }

    if (bsp_adc_regular_refresh() == 0U) {
        s_adc1_polling_refresh_tick = now;
        s_adc1_polling_refresh_valid = 1U;
    }
}

uint8_t bsp_adc_motor_get_adc1_mode(void)
{
    return s_adc1_mode;
}

uint8_t bsp_adc_motor_use_polling_backup(void)
{
    uint8_t status;

    if (s_adc1_mode == BSP_ADC1_MODE_DMA) {
        (void)HAL_ADC_Stop_DMA(&hadc1);
    }

    s_adc1_dma_buf[0] = 0U;
    s_adc1_dma_buf[1] = 0U;
    s_adc1_polling_refresh_tick = 0U;
    s_adc1_polling_refresh_valid = 0U;

    status = bsp_adc_regular_refresh();
    if (status == 0U) {
        s_adc1_mode = BSP_ADC1_MODE_POLLING;
        s_adc1_polling_refresh_tick = HAL_GetTick();
        s_adc1_polling_refresh_valid = 1U;
    }

    return status;
}

uint8_t bsp_adc_motor_init(void)
{
    uint8_t backup_status;

    s_adc1_dma_buf[0] = 0U;
    s_adc1_dma_buf[1] = 0U;
    s_phase_u_raw = 0U;
    s_phase_v_raw = 0U;
    s_phase_w_raw = 0U;
    s_injected_update_count = 0U;
    s_adc1_mode = BSP_ADC1_MODE_DMA;
    s_adc1_polling_refresh_tick = 0U;
    s_adc1_polling_refresh_valid = 0U;

    if (HAL_ADCEx_Calibration_Start(&hadc1, ADC_SINGLE_ENDED) != HAL_OK) {
        return BSP_ADC_ERROR_ADC1_CAL;
    }

    if (HAL_ADCEx_Calibration_Start(&hadc2, ADC_SINGLE_ENDED) != HAL_OK) {
        return BSP_ADC_ERROR_ADC2_CAL;
    }

    if (HAL_ADC_Start_DMA(&hadc1, (uint32_t *)(void *)s_adc1_dma_buf, 2U) != HAL_OK) {
        backup_status = bsp_adc_motor_use_polling_backup();
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
    bsp_adc_polling_refresh_if_needed();
    return s_adc1_dma_buf[0];
}

uint16_t bsp_adc_get_temp_raw(void)
{
    bsp_adc_polling_refresh_if_needed();
    return s_adc1_dma_buf[1];
}

float bsp_adc_get_vbus(void)
{
    return ((float)bsp_adc_get_vbus_raw() / BSP_ADC_FULL_SCALE) *
           BSP_ADC_REF_VOLTAGE *
           BSP_ADC_VBUS_DIVIDER_RATIO *
           BSP_ADC_VBUS_CALIBRATION;
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

    return (1.0f / ((1.0f / BSP_ADC_TEMP_T0_KELVIN) +
           (logf(r_ntc / BSP_ADC_TEMP_R25_OHM) / BSP_ADC_TEMP_BETA))) - 273.15f;
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
    /* [Fix2] 读快照，三相数据来自同一次中断写入，不会撕裂 */
    phase_snapshot_t snap = {s_phase_snapshot.u, s_phase_snapshot.v, s_phase_snapshot.w};

    if (iu != NULL) {
        *iu = (int16_t)snap.u;
    }

    if (iv != NULL) {
        *iv = (int16_t)snap.v;
    }

    if (iw != NULL) {
        *iw = (int16_t)snap.w;
    }
}

uint32_t bsp_adc_get_injected_update_count(void)
{
    return s_injected_update_count;
}

void HAL_ADCEx_InjectedConvCpltCallback(ADC_HandleTypeDef *hadc)
{
    if (hadc->Instance == ADC2) {
        /* [Fix2] 先写入私有变量，再一次性更新快照，主循环读快照不会撕裂 */
        s_phase_u_raw = (uint16_t)HAL_ADCEx_InjectedGetValue(hadc, ADC_INJECTED_RANK_1);
        s_phase_v_raw = (uint16_t)HAL_ADCEx_InjectedGetValue(hadc, ADC_INJECTED_RANK_2);
        s_phase_w_raw = (uint16_t)HAL_ADCEx_InjectedGetValue(hadc, ADC_INJECTED_RANK_3);
        s_phase_snapshot.u = s_phase_u_raw;
        s_phase_snapshot.v = s_phase_v_raw;
        s_phase_snapshot.w = s_phase_w_raw;
        s_injected_update_count++;
    }
}
