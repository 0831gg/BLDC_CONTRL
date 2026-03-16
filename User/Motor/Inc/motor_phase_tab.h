/**
 * @file    motor_phase_tab.h
 * @brief   BLDC电机六步换相动作表
 * @details 本模块定义霍尔传感器状态到换相动作的映射表
 *          根据霍尔状态和旋转方向，查表得到应该导通的MOSFET组合
 *
 * @note    六步换相原理:
 *          BLDC电机三相绕组(U/V/W)，每步导通两相:
 *          - 一个相位的上桥臂输出PWM
 *          - 另一个相位的下桥臂导通
 *          - 第三相悬空
 *
 * @note    霍尔状态与换相动作对应关系:
 *          CW方向:  1→2→3→4→5→6
 *          CCW方向: 6→5→4→3→2→1
 *
 * @note    动作命名规则:
 *          MOTOR_PHASE_ACTION_UX_YZ = X相上桥臂PWM + Z相下桥臂导通
 *          例如: UP_VN = U相上桥臂PWM + V相下桥臂导通
 */

#ifndef __MOTOR_PHASE_TAB_H
#define __MOTOR_PHASE_TAB_H

#ifdef __cplusplus
extern "C" {
#endif

#include "main.h"

/**
 * @brief 换相动作枚举
 * @note 定义六步换相中各步的MOSFET导通组合
 */
typedef enum {
    MOTOR_PHASE_ACTION_INVALID = 0,  /**< @brief 无效动作 */
    MOTOR_PHASE_ACTION_UP_VN,         /**< @brief U相上桥PWM + V相下桥 */
    MOTOR_PHASE_ACTION_UP_WN,         /**< @brief U相上桥PWM + W相下桥 */
    MOTOR_PHASE_ACTION_VP_UN,         /**< @brief V相上桥PWM + U相下桥 */
    MOTOR_PHASE_ACTION_VP_WN,         /**< @brief V相上桥PWM + W相下桥 */
    MOTOR_PHASE_ACTION_WP_UN,         /**< @brief W相上桥PWM + U相下桥 */
    MOTOR_PHASE_ACTION_WP_VN          /**< @brief W相上桥PWM + V相下桥 */
} motor_phase_action_t;

/**
 * @brief 初始化默认换相表
 * @note 设置CW和CCW两个方向的换相映射
 */
void motor_phase_tab_init_defaults(void);

/**
 * @brief 设置换相表项
 * @param direction 方向
 * @param hall_state 霍尔状态
 * @param action 换相动作
 */
void motor_phase_tab_set(uint8_t direction, uint8_t hall_state, motor_phase_action_t action);

/**
 * @brief 获取换相动作
 * @param direction 方向
 * @param hall_state 霍尔状态
 * @return motor_phase_action_t 换相动作
 */
motor_phase_action_t motor_phase_tab_get(uint8_t direction, uint8_t hall_state);

/**
 * @brief 设置换相偏移
 * @param offset 偏移量(0-5)
 * @note 用于调整换相时机，优化电机性能
 */
void motor_phase_tab_set_commutation_offset(uint8_t offset);

/**
 * @brief 获取换相偏移
 * @return uint8_t 偏移量
 */
uint8_t motor_phase_tab_get_commutation_offset(void);

/**
 * @brief 获取动作名称字符串
 * @param action 动作
 * @return const char* 动作名称
 */
const char *motor_phase_action_name(motor_phase_action_t action);

/**
 * @brief 应用换相动作
 * @param action 换相动作
 * @param duty 占空比
 * @note 根据动作控制PWM和下桥臂
 */
void motor_phase_apply(motor_phase_action_t action, uint16_t duty);

/**
 * @brief U+V- 动作
 */
void mos_up_vn(uint16_t duty);

/**
 * @brief U+W- 动作
 */
void mos_up_wn(uint16_t duty);

/**
 * @brief V+U- 动作
 */
void mos_vp_un(uint16_t duty);

/**
 * @brief V+W- 动作
 */
void mos_vp_wn(uint16_t duty);

/**
 * @brief W+U- 动作
 */
void mos_wp_un(uint16_t duty);

/**
 * @brief W+V- 动作
 */
void mos_wp_vn(uint16_t duty);

#ifdef __cplusplus
}
#endif

#endif /* __MOTOR_PHASE_TAB_H */
