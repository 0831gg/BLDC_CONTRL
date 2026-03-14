/**
 * @file    bsp_pwm.h
 * @brief   TIM1 PWM and lower bridge control
 */

#ifndef __BSP_PWM_H
#define __BSP_PWM_H

#ifdef __cplusplus
extern "C" {
#endif

#include "main.h"
#include "tim.h"

#define BSP_PWM_PERIOD              (8399U)
#define BSP_PWM_DUTY_50_PERCENT     (BSP_PWM_PERIOD / 2U)

typedef enum {
    BSP_PWM_PHASE_NONE = 0,
    BSP_PWM_PHASE_U,
    BSP_PWM_PHASE_V,
    BSP_PWM_PHASE_W
} bsp_pwm_phase_t;

void bsp_pwm_init(void);
void bsp_pwm_duty_set(uint16_t duty);
uint16_t bsp_pwm_duty_get(void);
void bsp_pwm_all_open(void);
void bsp_pwm_all_close(void);
void bsp_pwm_ctrl_single_phase(bsp_pwm_phase_t phase);
void bsp_pwm_lower_set(uint8_t un, uint8_t vn, uint8_t wn);
void bsp_pwm_lower_all_off(void);
uint8_t bsp_pwm_is_break_fault(void);
void bsp_pwm_clear_break_fault(void);
void bsp_pwm_disable_break(void);

#ifdef __cplusplus
}
#endif

#endif /* __BSP_PWM_H */
