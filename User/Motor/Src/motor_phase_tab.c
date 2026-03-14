#include "motor_phase_tab.h"

#include "bsp_pwm.h"
#include "motor_ctrl.h"

static motor_phase_action_t s_phase_table[2][8];
static uint8_t s_commutation_offset = 0U;

static const uint8_t s_hall_sequence_cw[6] = {
    0x06U, 0x02U, 0x03U, 0x01U, 0x05U, 0x04U
};

static const uint8_t s_hall_sequence_ccw[6] = {
    0x06U, 0x04U, 0x05U, 0x01U, 0x03U, 0x02U
};

static uint8_t motor_phase_dir_index(uint8_t direction)
{
    return (direction == MOTOR_DIR_CCW) ? 1U : 0U;
}

static const uint8_t *motor_phase_get_sequence(uint8_t direction)
{
    return (direction == MOTOR_DIR_CCW) ? s_hall_sequence_ccw : s_hall_sequence_cw;
}

void motor_phase_tab_init_defaults(void)
{
    uint8_t i;

    s_commutation_offset = 2U;

    for (i = 0U; i < 8U; i++) {
        s_phase_table[0][i] = MOTOR_PHASE_ACTION_INVALID;
        s_phase_table[1][i] = MOTOR_PHASE_ACTION_INVALID;
    }

    s_phase_table[motor_phase_dir_index(MOTOR_DIR_CW)][0x06U] = MOTOR_PHASE_ACTION_UP_WN;  // Hall 6 → U+W-
    s_phase_table[motor_phase_dir_index(MOTOR_DIR_CW)][0x02U] = MOTOR_PHASE_ACTION_VP_UN;  // Hall 2 → V+U-
    s_phase_table[motor_phase_dir_index(MOTOR_DIR_CW)][0x03U] = MOTOR_PHASE_ACTION_VP_WN;  // Hall 3 → V+W-
    s_phase_table[motor_phase_dir_index(MOTOR_DIR_CW)][0x01U] = MOTOR_PHASE_ACTION_WP_VN;  // Hall 1 → W+V-
    s_phase_table[motor_phase_dir_index(MOTOR_DIR_CW)][0x05U] = MOTOR_PHASE_ACTION_UP_VN;  // Hall 5 → U+V-
    s_phase_table[motor_phase_dir_index(MOTOR_DIR_CW)][0x04U] = MOTOR_PHASE_ACTION_WP_UN;  // Hall 4 → W+U-

    s_phase_table[motor_phase_dir_index(MOTOR_DIR_CCW)][0x06U] = MOTOR_PHASE_ACTION_VP_UN;  // Hall 6 → V+U-
    s_phase_table[motor_phase_dir_index(MOTOR_DIR_CCW)][0x04U] = MOTOR_PHASE_ACTION_UP_WN;  // Hall 4 → U+W-
    s_phase_table[motor_phase_dir_index(MOTOR_DIR_CCW)][0x05U] = MOTOR_PHASE_ACTION_UP_VN;  // Hall 5 → U+V-
    s_phase_table[motor_phase_dir_index(MOTOR_DIR_CCW)][0x01U] = MOTOR_PHASE_ACTION_WP_VN;  // Hall 1 → W+V-
    s_phase_table[motor_phase_dir_index(MOTOR_DIR_CCW)][0x03U] = MOTOR_PHASE_ACTION_VP_WN;  // Hall 3 → V+W-
    s_phase_table[motor_phase_dir_index(MOTOR_DIR_CCW)][0x02U] = MOTOR_PHASE_ACTION_VP_UN;  // Hall 2 → V+U-
}

void motor_phase_tab_set(uint8_t direction, uint8_t hall_state, motor_phase_action_t action)
{
    if (hall_state < 8U) {
        s_phase_table[motor_phase_dir_index(direction)][hall_state] = action;
    }
}

motor_phase_action_t motor_phase_tab_get(uint8_t direction, uint8_t hall_state)
{
    const uint8_t *sequence;
    uint8_t i;

    if (hall_state < 8U) {
        if (s_commutation_offset == 0U) {
            return s_phase_table[motor_phase_dir_index(direction)][hall_state];
        }

        sequence = motor_phase_get_sequence(direction);
        for (i = 0U; i < 6U; i++) {
            if (sequence[i] == hall_state) {
                return s_phase_table[motor_phase_dir_index(direction)]
                                    [sequence[(uint8_t)((i + s_commutation_offset) % 6U)]];
            }
        }
    }

    return MOTOR_PHASE_ACTION_INVALID;
}

void motor_phase_tab_set_commutation_offset(uint8_t offset)
{
    s_commutation_offset = (uint8_t)(offset % 6U);
}

uint8_t motor_phase_tab_get_commutation_offset(void)
{
    return s_commutation_offset;
}

const char *motor_phase_action_name(motor_phase_action_t action)
{
    switch (action) {
        case MOTOR_PHASE_ACTION_UP_VN: return "U+V-";
        case MOTOR_PHASE_ACTION_UP_WN: return "U+W-";
        case MOTOR_PHASE_ACTION_VP_UN: return "V+U-";
        case MOTOR_PHASE_ACTION_VP_WN: return "V+W-";
        case MOTOR_PHASE_ACTION_WP_UN: return "W+U-";
        case MOTOR_PHASE_ACTION_WP_VN: return "W+V-";
        case MOTOR_PHASE_ACTION_INVALID:
        default:
            return "invalid";
    }
}

void motor_phase_apply(motor_phase_action_t action, uint16_t duty)
{
    switch (action) {
        case MOTOR_PHASE_ACTION_UP_VN:
            mos_up_vn(duty);
            break;
        case MOTOR_PHASE_ACTION_UP_WN:
            mos_up_wn(duty);
            break;
        case MOTOR_PHASE_ACTION_VP_UN:
            mos_vp_un(duty);
            break;
        case MOTOR_PHASE_ACTION_VP_WN:
            mos_vp_wn(duty);
            break;
        case MOTOR_PHASE_ACTION_WP_UN:
            mos_wp_un(duty);
            break;
        case MOTOR_PHASE_ACTION_WP_VN:
            mos_wp_vn(duty);
            break;
        case MOTOR_PHASE_ACTION_INVALID:
        default:
            bsp_pwm_lower_all_off();
            bsp_pwm_ctrl_single_phase(BSP_PWM_PHASE_NONE);
            break;
    }
}

void mos_up_vn(uint16_t duty)
{
    bsp_pwm_duty_set(duty);
    bsp_pwm_lower_all_off();
    bsp_pwm_ctrl_single_phase(BSP_PWM_PHASE_U);
    bsp_pwm_lower_set(0U, 1U, 0U);
}

void mos_up_wn(uint16_t duty)
{
    bsp_pwm_duty_set(duty);
    bsp_pwm_lower_all_off();
    bsp_pwm_ctrl_single_phase(BSP_PWM_PHASE_U);
    bsp_pwm_lower_set(0U, 0U, 1U);
}

void mos_vp_un(uint16_t duty)
{
    bsp_pwm_duty_set(duty);
    bsp_pwm_lower_all_off();
    bsp_pwm_ctrl_single_phase(BSP_PWM_PHASE_V);
    bsp_pwm_lower_set(1U, 0U, 0U);
}

void mos_vp_wn(uint16_t duty)
{
    bsp_pwm_duty_set(duty);
    bsp_pwm_lower_all_off();
    bsp_pwm_ctrl_single_phase(BSP_PWM_PHASE_V);
    bsp_pwm_lower_set(0U, 0U, 1U);
}

void mos_wp_un(uint16_t duty)
{
    bsp_pwm_duty_set(duty);
    bsp_pwm_lower_all_off();
    bsp_pwm_ctrl_single_phase(BSP_PWM_PHASE_W);
    bsp_pwm_lower_set(1U, 0U, 0U);
}

void mos_wp_vn(uint16_t duty)
{
    bsp_pwm_duty_set(duty);
    bsp_pwm_lower_all_off();
    bsp_pwm_ctrl_single_phase(BSP_PWM_PHASE_W);
    bsp_pwm_lower_set(0U, 1U, 0U);
}
