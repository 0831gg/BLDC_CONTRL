/**
 * @file    motor_ctrl.h
 * @brief   Phase 4 motor start/stop and Hall commutation control
 */

#ifndef __MOTOR_CTRL_H
#define __MOTOR_CTRL_H

#ifdef __cplusplus
extern "C" {
#endif

#include "main.h"
#include "motor_phase_tab.h"

#define MOTOR_DIR_CW               (0U)
#define MOTOR_DIR_CCW              (1U)

#define MOTOR_FAULT_NONE           (0U)
#define MOTOR_FAULT_BREAK          (1U)
#define MOTOR_FAULT_HALL_INVALID   (2U)
#define MOTOR_FAULT_START_TIMEOUT  (3U)

void motor_ctrl_init(void);
int motor_start(uint16_t duty, uint8_t direction);
int motor_start_assisted(uint16_t duty, uint8_t direction);
int motor_start_simple(uint16_t duty, uint8_t direction);
void motor_stop(void);
void motor_sensor_mode_phase(void);
void motor_ctrl_set_fault(uint8_t fault);
uint8_t motor_ctrl_get_fault(void);
uint8_t motor_ctrl_is_running(void);
uint8_t motor_ctrl_get_direction(void);
void motor_ctrl_set_direction(uint8_t direction);
uint16_t motor_ctrl_get_duty(void);
void motor_ctrl_set_duty(uint16_t duty);
uint8_t motor_ctrl_get_hall_state(void);
motor_phase_action_t motor_ctrl_get_last_action(void);
const char *motor_ctrl_get_startup_stage_text(void);
uint8_t motor_ctrl_get_startup_step(void);
uint8_t motor_ctrl_get_startup_hall_state(void);
motor_phase_action_t motor_ctrl_get_startup_action(void);
uint16_t motor_ctrl_get_startup_duty(void);

#ifdef __cplusplus
}
#endif

#endif /* __MOTOR_CTRL_H */
