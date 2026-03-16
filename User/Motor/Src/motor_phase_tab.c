/**
 * @file    motor_phase_tab.c
 * @brief   六步换相表实现
 * @details 实现霍尔状态到换相动作的映射和查表功能
 * @see     motor_phase_tab.h
 */

#include "motor_phase_tab.h"

#include "bsp_pwm.h"
#include "motor_ctrl.h"

/*============================================================================
 * 私有变量定义
 *============================================================================*/

/** @brief 换相表 [方向][霍尔状态] */
static motor_phase_action_t s_phase_table[2][8];
/** @brief 换相偏移量 */
static uint8_t s_commutation_offset = 0U;

/** @brief 顺时针霍尔序列 */
static const uint8_t s_hall_sequence_cw[6] = {
    0x01U, 0x02U, 0x03U, 0x04U, 0x05U, 0x06U  /* 参考实现：1→2→3→4→5→6 */
};

/** @brief 逆时针霍尔序列 */
static const uint8_t s_hall_sequence_ccw[6] = {
    0x06U, 0x05U, 0x04U, 0x03U, 0x02U, 0x01U  /* 参考实现：6→5→4→3→2→1 (7-CW) */
};

/*============================================================================
 * 私有函数实现
 *============================================================================*/

/**
 * @brief 获取方向索引
 * @return 0:CCW, 1:CW
 */
static uint8_t motor_phase_dir_index(uint8_t direction)
{
    return (direction == MOTOR_DIR_CW) ? 1U : 0U;
}

/**
 * @brief 获取指定方向的霍尔序列
 * @return 序列指针
 */
static const uint8_t *motor_phase_get_sequence(uint8_t direction)
{
    return (direction == MOTOR_DIR_CW) ? s_hall_sequence_cw : s_hall_sequence_ccw;
}

/*============================================================================
 * 公共函数实现
 *============================================================================*/

/**
 * @brief 初始化默认换相表
 * @note 建立CW和CCW两个方向的霍尔状态到动作的映射
 */
void motor_phase_tab_init_defaults(void)
{
    uint8_t i;

    s_commutation_offset = 0U;  /* 先禁用offset，使用直接换相 */

    /* 初始化所有状态为无效 */
    for (i = 0U; i < 8U; i++) {
        s_phase_table[0][i] = MOTOR_PHASE_ACTION_INVALID;
        s_phase_table[1][i] = MOTOR_PHASE_ACTION_INVALID;
    }

    /* CW换相表（参考实现） */
    s_phase_table[motor_phase_dir_index(MOTOR_DIR_CW)][0x01U] = MOTOR_PHASE_ACTION_UP_WN;  /* Hall 1 → U+W- */
    s_phase_table[motor_phase_dir_index(MOTOR_DIR_CW)][0x02U] = MOTOR_PHASE_ACTION_VP_UN;  /* Hall 2 → V+U- */
    s_phase_table[motor_phase_dir_index(MOTOR_DIR_CW)][0x03U] = MOTOR_PHASE_ACTION_VP_WN;  /* Hall 3 → V+W- */
    s_phase_table[motor_phase_dir_index(MOTOR_DIR_CW)][0x04U] = MOTOR_PHASE_ACTION_WP_VN;  /* Hall 4 → W+V- */
    s_phase_table[motor_phase_dir_index(MOTOR_DIR_CW)][0x05U] = MOTOR_PHASE_ACTION_UP_VN;  /* Hall 5 → U+V- */
    s_phase_table[motor_phase_dir_index(MOTOR_DIR_CW)][0x06U] = MOTOR_PHASE_ACTION_WP_UN;  /* Hall 6 → W+U- */

    /* CCW换相表（7-hall反转，参考实现） */
    s_phase_table[motor_phase_dir_index(MOTOR_DIR_CCW)][0x06U] = MOTOR_PHASE_ACTION_UP_WN;  /* Hall 6 (7-1) → U+W- */
    s_phase_table[motor_phase_dir_index(MOTOR_DIR_CCW)][0x05U] = MOTOR_PHASE_ACTION_VP_UN;  /* Hall 5 (7-2) → V+U- */
    s_phase_table[motor_phase_dir_index(MOTOR_DIR_CCW)][0x04U] = MOTOR_PHASE_ACTION_VP_WN;  /* Hall 4 (7-3) → V+W- */
    s_phase_table[motor_phase_dir_index(MOTOR_DIR_CCW)][0x03U] = MOTOR_PHASE_ACTION_WP_VN;  /* Hall 3 (7-4) → W+V- */
    s_phase_table[motor_phase_dir_index(MOTOR_DIR_CCW)][0x02U] = MOTOR_PHASE_ACTION_UP_VN;  /* Hall 2 (7-5) → U+V- */
    s_phase_table[motor_phase_dir_index(MOTOR_DIR_CCW)][0x01U] = MOTOR_PHASE_ACTION_WP_UN;  /* Hall 1 (7-6) → W+U- */
}

/**
 * @brief 设置换相表项
 */
void motor_phase_tab_set(uint8_t direction, uint8_t hall_state, motor_phase_action_t action)
{
    if (hall_state < 8U) {
        s_phase_table[motor_phase_dir_index(direction)][hall_state] = action;
    }
}

/**
 * @brief 获取换相动作
 */
motor_phase_action_t motor_phase_tab_get(uint8_t direction, uint8_t hall_state)
{
    const uint8_t *sequence;
    uint8_t i;

    if (hall_state < 8U) {
        /* 无偏移时直接查表 */
        if (s_commutation_offset == 0U) {
            return s_phase_table[motor_phase_dir_index(direction)][hall_state];
        }

        /* 有偏移时查找序列位置并应用偏移 */
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

/**
 * @brief 设置换相偏移
 */
void motor_phase_tab_set_commutation_offset(uint8_t offset)
{
    s_commutation_offset = (uint8_t)(offset % 6U);
}

/**
 * @brief 获取换相偏移
 */
uint8_t motor_phase_tab_get_commutation_offset(void)
{
    return s_commutation_offset;
}

/**
 * @brief 获取动作名称
 */
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

/**
 * @brief 应用换相动作
 */
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

/**
 * @brief U+V- 换相动作
 * @note U相上桥PWM输出，V相下桥导通
 */
void mos_up_vn(uint16_t duty)
{
    bsp_pwm_duty_set(duty);
    bsp_pwm_lower_all_off();
    bsp_pwm_ctrl_single_phase(BSP_PWM_PHASE_U);
    bsp_pwm_lower_set(0U, 1U, 0U);
}

/**
 * @brief U+W- 换相动作
 */
void mos_up_wn(uint16_t duty)
{
    bsp_pwm_duty_set(duty);
    bsp_pwm_lower_all_off();
    bsp_pwm_ctrl_single_phase(BSP_PWM_PHASE_U);
    bsp_pwm_lower_set(0U, 0U, 1U);
}

/**
 * @brief V+U- 换相动作
 */
void mos_vp_un(uint16_t duty)
{
    bsp_pwm_duty_set(duty);
    bsp_pwm_lower_all_off();
    bsp_pwm_ctrl_single_phase(BSP_PWM_PHASE_V);
    bsp_pwm_lower_set(1U, 0U, 0U);
}

/**
 * @brief V+W- 换相动作
 */
void mos_vp_wn(uint16_t duty)
{
    bsp_pwm_duty_set(duty);
    bsp_pwm_lower_all_off();
    bsp_pwm_ctrl_single_phase(BSP_PWM_PHASE_V);
    bsp_pwm_lower_set(0U, 0U, 1U);
}

/**
 * @brief W+U- 换相动作
 */
void mos_wp_un(uint16_t duty)
{
    bsp_pwm_duty_set(duty);
    bsp_pwm_lower_all_off();
    bsp_pwm_ctrl_single_phase(BSP_PWM_PHASE_W);
    bsp_pwm_lower_set(1U, 0U, 0U);
}

/**
 * @brief W+V- 换相动作
 */
void mos_wp_vn(uint16_t duty)
{
    bsp_pwm_duty_set(duty);
    bsp_pwm_lower_all_off();
    bsp_pwm_ctrl_single_phase(BSP_PWM_PHASE_W);
    bsp_pwm_lower_set(0U, 1U, 0U);
}
