#include "motor_ctrl.h"

#include "bsp_ctrl_sd.h"
#include "bsp_delay.h"
#include "bsp_hall.h"
#include "bsp_pwm.h"

#define MOTOR_START_PREPARE_DELAY_MS    (2U)
#define MOTOR_START_ALIGN_DELAY_MS      (80U)
#define MOTOR_START_ASSIST_TIMEOUT_MS   (300U)
#define MOTOR_START_ASSIST_DUTY_STEP1   (((BSP_PWM_PERIOD + 1U) * 5U) / 100U)
#define MOTOR_START_ASSIST_DUTY_STEP2   (((BSP_PWM_PERIOD + 1U) * 8U) / 100U)
#define MOTOR_START_ASSIST_DUTY_STEP3   (((BSP_PWM_PERIOD + 1U) * 10U) / 100U)

static const uint8_t s_start_assist_step_delay_ms[] = {20U, 16U, 14U, 12U, 10U, 8U};

typedef enum {
    MOTOR_STARTUP_STAGE_IDLE = 0,
    MOTOR_STARTUP_STAGE_PREPARE,
    MOTOR_STARTUP_STAGE_ALIGN,
    MOTOR_STARTUP_STAGE_KICK,
    MOTOR_STARTUP_STAGE_WAIT_EDGE,
    MOTOR_STARTUP_STAGE_TRACKING
} motor_startup_stage_t;

static uint8_t s_motor_running = 0U;
static uint8_t s_motor_direction = MOTOR_DIR_CW;
static uint8_t s_motor_fault = MOTOR_FAULT_NONE;
static uint16_t s_motor_duty = 0U;
static uint8_t s_last_hall_state = 0U;
static motor_phase_action_t s_last_action = MOTOR_PHASE_ACTION_INVALID;
static uint8_t s_startup_stage = MOTOR_STARTUP_STAGE_IDLE;
static uint8_t s_startup_step = 0U;
static uint8_t s_startup_hall_state = 0U;
static motor_phase_action_t s_startup_action = MOTOR_PHASE_ACTION_INVALID;
static uint16_t s_startup_duty = 0U;

static void motor_ctrl_set_startup_trace(uint8_t stage,
                                         uint8_t step,
                                         uint8_t hall_state,
                                         motor_phase_action_t action,
                                         uint16_t duty)
{
    s_startup_stage = stage;
    s_startup_step = step;
    s_startup_hall_state = hall_state;
    s_startup_action = action;
    s_startup_duty = duty;
}

static void motor_ctrl_prepare_start(uint16_t duty, uint8_t direction)
{
    s_motor_fault = MOTOR_FAULT_NONE;
    s_motor_direction = direction;
    s_motor_duty = duty;

    bsp_hall_clear_stats();
    bsp_ctrl_sd_disable();
    bsp_pwm_clear_break_fault();
    bsp_pwm_lower_all_off();
    /* 阶段2：删除bsp_pwm_all_close()和bsp_pwm_all_open()，PWM保持运行 */
    bsp_pwm_duty_set(s_motor_duty);
    s_motor_running = 1U;
    motor_ctrl_set_startup_trace(MOTOR_STARTUP_STAGE_PREPARE,
                                 0U,
                                 bsp_hall_get_state(),
                                 MOTOR_PHASE_ACTION_INVALID,
                                 s_motor_duty);
}

static uint16_t motor_ctrl_get_assist_duty(uint16_t duty, uint8_t step)
{
    (void)duty;

    if (step < 2U) {
        return MOTOR_START_ASSIST_DUTY_STEP1;
    } else if (step < 4U) {
        return MOTOR_START_ASSIST_DUTY_STEP2;
    } else {
        return MOTOR_START_ASSIST_DUTY_STEP3;
    }
}

static uint8_t motor_ctrl_get_next_hall_state(uint8_t direction, uint8_t state)
{
    if (direction == MOTOR_DIR_CCW) {
        switch (state) {
            case 0x06U: return 0x04U;
            case 0x04U: return 0x05U;
            case 0x05U: return 0x01U;
            case 0x01U: return 0x03U;
            case 0x03U: return 0x02U;
            case 0x02U: return 0x06U;
            default:    return 0x00U;
        }
    }

    switch (state) {
        case 0x06U: return 0x02U;
        case 0x02U: return 0x03U;
        case 0x03U: return 0x01U;
        case 0x01U: return 0x05U;
        case 0x05U: return 0x04U;
        case 0x04U: return 0x06U;
        default:    return 0x00U;
    }
}

static int motor_ctrl_apply_hall_action(uint8_t direction, uint8_t hall_state, uint16_t duty)
{
    motor_phase_action_t action = motor_phase_tab_get(direction, hall_state);

    s_last_hall_state = hall_state;
    s_last_action = action;

    if (action == MOTOR_PHASE_ACTION_INVALID) {
        motor_ctrl_set_fault(MOTOR_FAULT_HALL_INVALID);
        bsp_pwm_lower_all_off();
        bsp_pwm_ctrl_single_phase(BSP_PWM_PHASE_NONE);
        return -1;
    }

    motor_phase_apply(action, duty);
    return 0;
}

static uint8_t motor_ctrl_check_break_fault(void)
{
    if (bsp_pwm_is_break_fault() == 0U) {
        return 0U;
    }

    bsp_pwm_clear_break_fault();
    motor_ctrl_set_fault(MOTOR_FAULT_BREAK);
    return 1U;
}

void motor_ctrl_init(void)
{
    motor_phase_tab_init_defaults();
    s_motor_running = 0U;
    s_motor_direction = MOTOR_DIR_CW;
    s_motor_fault = MOTOR_FAULT_NONE;
    s_motor_duty = 0U;
    s_last_hall_state = 0U;
    s_last_action = MOTOR_PHASE_ACTION_INVALID;
    motor_ctrl_set_startup_trace(MOTOR_STARTUP_STAGE_IDLE,
                                 0U,
                                 0U,
                                 MOTOR_PHASE_ACTION_INVALID,
                                 0U);

    bsp_pwm_lower_all_off();
    bsp_pwm_all_close();
    bsp_ctrl_sd_disable();
}

int motor_start(uint16_t duty, uint8_t direction)
{
    motor_ctrl_prepare_start(duty, direction);

    motor_sensor_mode_phase();
    Delay_ms(MOTOR_START_PREPARE_DELAY_MS);

    if ((s_motor_fault != MOTOR_FAULT_NONE) ||
        (motor_ctrl_check_break_fault() != 0U)) {
        motor_stop();
        return -1;
    }

    bsp_ctrl_sd_enable();
    return 0;
}

int motor_start_assisted(uint16_t duty, uint8_t direction)
{
    uint16_t requested_duty = duty;
    uint16_t assist_duty;
    uint8_t assist_state;
    motor_phase_action_t assist_action;
    uint32_t start_tick;
    uint32_t i;

    assist_duty = motor_ctrl_get_assist_duty(duty, 0U);
    motor_ctrl_prepare_start(assist_duty, direction);

    assist_state = bsp_hall_get_state();
    assist_action = motor_phase_tab_get(direction, assist_state);
    motor_ctrl_set_startup_trace(MOTOR_STARTUP_STAGE_ALIGN,
                                 0U,
                                 assist_state,
                                 assist_action,
                                 assist_duty);
    if (motor_ctrl_apply_hall_action(direction, assist_state, assist_duty) != 0) {
        motor_stop();
        return -1;
    }

    bsp_ctrl_sd_enable();
    Delay_ms(MOTOR_START_ALIGN_DELAY_MS);

    if ((s_motor_fault != MOTOR_FAULT_NONE) ||
        (motor_ctrl_check_break_fault() != 0U)) {
        motor_stop();
        return -1;
    }

    for (i = 0U; i < (sizeof(s_start_assist_step_delay_ms) / sizeof(s_start_assist_step_delay_ms[0])); i++) {
        assist_duty = motor_ctrl_get_assist_duty(duty, (uint8_t)i);
        assist_state = motor_ctrl_get_next_hall_state(direction, assist_state);
        if (assist_state == 0U) {
            motor_ctrl_set_fault(MOTOR_FAULT_HALL_INVALID);
            motor_stop();
            return -1;
        }

        assist_action = motor_phase_tab_get(direction, assist_state);
        motor_ctrl_set_startup_trace(MOTOR_STARTUP_STAGE_KICK,
                                     (uint8_t)(i + 1U),
                                     assist_state,
                                     assist_action,
                                     assist_duty);
        if (motor_ctrl_apply_hall_action(direction, assist_state, assist_duty) != 0) {
            motor_stop();
            return -1;
        }

        Delay_ms(s_start_assist_step_delay_ms[i]);

        if ((s_motor_fault != MOTOR_FAULT_NONE) ||
            (motor_ctrl_check_break_fault() != 0U)) {
            motor_stop();
            return -1;
        }
    }

    bsp_hall_clear_stats();
    motor_sensor_mode_phase();
    motor_ctrl_set_startup_trace(MOTOR_STARTUP_STAGE_WAIT_EDGE,
                                 0U,
                                 s_last_hall_state,
                                 s_last_action,
                                 assist_duty);
    if ((s_motor_fault != MOTOR_FAULT_NONE) ||
        (motor_ctrl_check_break_fault() != 0U)) {
        motor_stop();
        return -1;
    }

    start_tick = HAL_GetTick();

    while ((HAL_GetTick() - start_tick) < MOTOR_START_ASSIST_TIMEOUT_MS) {
        if (bsp_hall_get_valid_transition_count() != 0U) {
            s_motor_duty = requested_duty;
            motor_sensor_mode_phase();
            motor_ctrl_set_startup_trace(MOTOR_STARTUP_STAGE_TRACKING,
                                         0U,
                                         s_last_hall_state,
                                         s_last_action,
                                         requested_duty);

            if ((s_motor_fault != MOTOR_FAULT_NONE) ||
                (motor_ctrl_check_break_fault() != 0U)) {
                motor_stop();
                return -1;
            }

            return 0;
        }

        Delay_ms(1U);
    }

    motor_ctrl_set_fault(MOTOR_FAULT_START_TIMEOUT);
    motor_stop();
    return -1;
}

int motor_start_simple(uint16_t duty, uint8_t direction)
{
    s_motor_fault = MOTOR_FAULT_NONE;
    s_motor_direction = direction;
    s_motor_duty = duty;

    bsp_hall_clear_stats();
    bsp_pwm_clear_break_fault();
    bsp_pwm_duty_set(s_motor_duty);

    s_motor_running = 1U;
    motor_ctrl_set_startup_trace(MOTOR_STARTUP_STAGE_TRACKING,
                                 0U,
                                 bsp_hall_get_state(),
                                 MOTOR_PHASE_ACTION_INVALID,
                                 s_motor_duty);

    motor_sensor_mode_phase();

    if (s_motor_fault != MOTOR_FAULT_NONE) {
        motor_stop();
        return -1;
    }

    bsp_ctrl_sd_enable();

    return 0;
}

void motor_stop(void)
{
    bsp_ctrl_sd_disable();
    bsp_pwm_lower_all_off();
    bsp_pwm_all_close();
    s_motor_running = 0U;
    s_last_action = MOTOR_PHASE_ACTION_INVALID;
}

void motor_sensor_mode_phase(void)
{
    motor_phase_action_t action;
    uint8_t hall_state = bsp_hall_get_state();

    s_last_hall_state = hall_state;
    action = motor_phase_tab_get(s_motor_direction, hall_state);
    s_last_action = action;

    if (s_motor_running == 0U) {
        return;
    }

    if (action == MOTOR_PHASE_ACTION_INVALID) {
        motor_ctrl_set_fault(MOTOR_FAULT_HALL_INVALID);
        bsp_pwm_lower_all_off();
        bsp_pwm_ctrl_single_phase(BSP_PWM_PHASE_NONE);
        return;
    }

    motor_phase_apply(action, s_motor_duty);
}

void motor_ctrl_set_fault(uint8_t fault)
{
    s_motor_fault = fault;
}

uint8_t motor_ctrl_get_fault(void)
{
    return s_motor_fault;
}

uint8_t motor_ctrl_is_running(void)
{
    return s_motor_running;
}

uint8_t motor_ctrl_get_direction(void)
{
    return s_motor_direction;
}

void motor_ctrl_set_direction(uint8_t direction)
{
    s_motor_direction = direction;
}

uint16_t motor_ctrl_get_duty(void)
{
    return s_motor_duty;
}

void motor_ctrl_set_duty(uint16_t duty)
{
    s_motor_duty = duty;

    if (s_motor_running != 0U) {
        motor_sensor_mode_phase();
    }
}

uint8_t motor_ctrl_get_hall_state(void)
{
    return s_last_hall_state;
}

motor_phase_action_t motor_ctrl_get_last_action(void)
{
    return s_last_action;
}

const char *motor_ctrl_get_startup_stage_text(void)
{
    switch (s_startup_stage) {
        case MOTOR_STARTUP_STAGE_PREPARE:
            return "prepare";
        case MOTOR_STARTUP_STAGE_ALIGN:
            return "align";
        case MOTOR_STARTUP_STAGE_KICK:
            return "kick";
        case MOTOR_STARTUP_STAGE_WAIT_EDGE:
            return "wait_edge";
        case MOTOR_STARTUP_STAGE_TRACKING:
            return "tracking";
        case MOTOR_STARTUP_STAGE_IDLE:
        default:
            return "idle";
    }
}

uint8_t motor_ctrl_get_startup_step(void)
{
    return s_startup_step;
}

uint8_t motor_ctrl_get_startup_hall_state(void)
{
    return s_startup_hall_state;
}

motor_phase_action_t motor_ctrl_get_startup_action(void)
{
    return s_startup_action;
}

uint16_t motor_ctrl_get_startup_duty(void)
{
    return s_startup_duty;
}
