/**
 * @file    motor_phase_tab.h
 * @brief   Six-step commutation action table
 */

#ifndef __MOTOR_PHASE_TAB_H
#define __MOTOR_PHASE_TAB_H

#ifdef __cplusplus
extern "C" {
#endif

#include "main.h"

typedef enum {
    MOTOR_PHASE_ACTION_INVALID = 0,
    MOTOR_PHASE_ACTION_UP_VN,
    MOTOR_PHASE_ACTION_UP_WN,
    MOTOR_PHASE_ACTION_VP_UN,
    MOTOR_PHASE_ACTION_VP_WN,
    MOTOR_PHASE_ACTION_WP_UN,
    MOTOR_PHASE_ACTION_WP_VN
} motor_phase_action_t;

void motor_phase_tab_init_defaults(void);
void motor_phase_tab_set(uint8_t direction, uint8_t hall_state, motor_phase_action_t action);
motor_phase_action_t motor_phase_tab_get(uint8_t direction, uint8_t hall_state);
void motor_phase_tab_set_commutation_offset(uint8_t offset);
uint8_t motor_phase_tab_get_commutation_offset(void);
const char *motor_phase_action_name(motor_phase_action_t action);
void motor_phase_apply(motor_phase_action_t action, uint16_t duty);
void mos_up_vn(uint16_t duty);
void mos_up_wn(uint16_t duty);
void mos_vp_un(uint16_t duty);
void mos_vp_wn(uint16_t duty);
void mos_wp_un(uint16_t duty);
void mos_wp_vn(uint16_t duty);

#ifdef __cplusplus
}
#endif

#endif /* __MOTOR_PHASE_TAB_H */
