/**
 * @file    bsp_adc_motor.h
 * @brief   ADC1/ADC2 motor sampling support
 */

#ifndef __BSP_ADC_MOTOR_H
#define __BSP_ADC_MOTOR_H

#ifdef __cplusplus
extern "C" {
#endif

#include "main.h"

#define BSP_ADC1_MODE_DMA       (0U)
#define BSP_ADC1_MODE_POLLING   (1U)

uint8_t bsp_adc_motor_init(void);
uint8_t bsp_adc_motor_get_adc1_mode(void);
uint8_t bsp_adc_motor_use_polling_backup(void);
uint16_t bsp_adc_get_vbus_raw(void);
uint16_t bsp_adc_get_temp_raw(void);
float bsp_adc_get_vbus(void);
float bsp_adc_get_temp(void);
uint16_t bsp_adc_get_phase_u_raw(void);
uint16_t bsp_adc_get_phase_v_raw(void);
uint16_t bsp_adc_get_phase_w_raw(void);
void bsp_adc_get_phase_current(int16_t *iu, int16_t *iv, int16_t *iw);
uint32_t bsp_adc_get_injected_update_count(void);

#ifdef __cplusplus
}
#endif

#endif /* __BSP_ADC_MOTOR_H */
