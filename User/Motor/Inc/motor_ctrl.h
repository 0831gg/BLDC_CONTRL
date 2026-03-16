/**
 * @file    motor_ctrl.h
 * @brief   BLDC电机控制接口
 * @details 本模块提供BLDC电机的启动、停止、换相控制功能
 *          支持有传感器（霍尔）六步换向控制
 *
 * @note    电机启动模式:
 *          - motor_start(): 简单启动
 *          - motor_start_assisted(): 带预定位和强启动
 *          - motor_start_simple(): 最简启动（生产版本）
 *
 * @note    电机控制流程:
 *          1. 初始化: motor_ctrl_init()
 *          2. 启动: motor_start_simple(duty, direction)
 *          3. 换相: 霍尔传感器检测→motor_sensor_mode_phase()
 *          4. 停止: motor_stop()
 *
 * @note    故障类型:
 *          - MOTOR_FAULT_NONE: 无故障
 *          - MOTOR_FAULT_BREAK: 硬件故障（过流等）
 *          - MOTOR_FAULT_HALL_INVALID: 霍尔状态无效
 *          - MOTOR_FAULT_START_TIMEOUT: 启动超时
 */

#ifndef __MOTOR_CTRL_H
#define __MOTOR_CTRL_H

#ifdef __cplusplus
extern "C" {
#endif

#include "main.h"
#include "motor_phase_tab.h"

/* 方向定义 */
#define MOTOR_DIR_CW               (1U)   /**< @brief 顺时针 */
#define MOTOR_DIR_CCW              (0U)   /**< @brief 逆时针 */

/* 故障类型 */
#define MOTOR_FAULT_NONE           (0U)   /**< @brief 无故障 */
#define MOTOR_FAULT_BREAK          (1U)   /**< @brief 硬件故障（Break触发） */
#define MOTOR_FAULT_HALL_INVALID   (2U)   /**< @brief 霍尔状态无效 */
#define MOTOR_FAULT_START_TIMEOUT  (3U)   /**< @brief 启动超时 */

/**
 * @brief 初始化电机控制模块
 * @note 初始化换相表，关闭所有输出
 */
void motor_ctrl_init(void);

/**
 * @brief 简单启动电机
 * @param duty 占空比
 * @param direction 方向 (MOTOR_DIR_CW 或 MOTOR_DIR_CCW)
 * @return 0:成功, -1:失败
 */
int motor_start(uint16_t duty, uint8_t direction);

/**
 * @brief 带辅助启动电机（预定位+强推动）
 * @param duty 占空比
 * @param direction 方向
 * @return 0:成功, -1:失败
 * @note 适用于高摩擦负载的启动
 */
int motor_start_assisted(uint16_t duty, uint8_t direction);

/**
 * @brief 简化启动电机（生产版本）
 * @param duty 占空比
 * @param direction 方向
 * @return 0:成功, -1:失败
 * @note 直接进入正常运行模式，适用于正常负载
 */
int motor_start_simple(uint16_t duty, uint8_t direction);

/**
 * @brief 停止电机
 * @note 禁用驱动芯片，关闭PWM输出
 */
void motor_stop(void);

/**
 * @brief 传感器模式换相（霍尔传感器触发）
 * @note 由bsp_hall_poll_and_commutate()调用
 *       根据当前霍尔状态查表得到换相动作
 */
void motor_sensor_mode_phase(void);

/**
 * @brief 设置故障标志
 * @param fault 故障类型
 */
void motor_ctrl_set_fault(uint8_t fault);

/**
 * @brief 获取当前故障状态
 * @return uint8_t 故障类型
 */
uint8_t motor_ctrl_get_fault(void);

/**
 * @brief 检查电机是否运行
 * @return uint8_t 1:运行中, 0:停止
 */
uint8_t motor_ctrl_is_running(void);

/**
 * @brief 获取当前方向
 * @return uint8_t 方向
 */
uint8_t motor_ctrl_get_direction(void);

/**
 * @brief 设置方向
 * @param direction 方向
 * @note 运行时切换方向可能导致电机抖动
 */
void motor_ctrl_set_direction(uint8_t direction);

/**
 * @brief 获取当前占空比
 * @return uint16_t 占空比值
 */
uint16_t motor_ctrl_get_duty(void);

/**
 * @brief 设置占空比
 * @param duty 占空比值
 * @note 运行中动态调整占空比
 */
void motor_ctrl_set_duty(uint16_t duty);

/**
 * @brief 获取当前霍尔状态
 * @return uint8_t 霍尔状态编码
 */
uint8_t motor_ctrl_get_hall_state(void);

/**
 * @brief 获取最近一次换相动作
 * @return motor_phase_action_t 换相动作
 */
motor_phase_action_t motor_ctrl_get_last_action(void);

/**
 * @brief 获取启动阶段文本描述
 * @return const char* 阶段名称
 */
const char *motor_ctrl_get_startup_stage_text(void);

/**
 * @brief 获取启动步骤编号
 * @return uint8_t 步骤号
 */
uint8_t motor_ctrl_get_startup_step(void);

/**
 * @brief 获取启动时的霍尔状态
 * @return uint8_t 霍尔状态
 */
uint8_t motor_ctrl_get_startup_hall_state(void);

/**
 * @brief 获取启动时的换相动作
 * @return motor_phase_action_t 动作
 */
motor_phase_action_t motor_ctrl_get_startup_action(void);

/**
 * @brief 获取启动时的占空比
 * @return uint16_t 占空比
 */
uint16_t motor_ctrl_get_startup_duty(void);

#ifdef __cplusplus
}
#endif

#endif /* __MOTOR_CTRL_H */
