/**
 * @file    bsp_hall.h
 * @brief   霍尔传感器输入采样与定时器支持
 * @details 本模块负责读取BLDC电机霍尔传感器的状态，通过TIM5定时器
 *          实现精确的换相时间测量和转速计算。采用轮询方式（非EXTI中断）
 *          检测霍尔状态变化，确保稳定的电机控制。
 *
 * @note    霍尔传感器三相定义:
 *          - HALL_U: 霍尔U相 (对应电机U相)
 *          - HALL_V: 霍尔V相 (对应电机V相)
 *          - HALL_W: 霍尔W相 (对应电机W相)
 *
 * @note    霍尔状态编码 (二进制: W V U):
 *          - 0x01 (001): 位置1
 *          - 0x03 (011): 位置2
 *          - 0x02 (010): 位置3
 *          - 0x06 (110): 位置4
 *          - 0x04 (100): 位置5
 *          - 0x05 (101): 位置6
 *
 * @note    旋转方向定义:
 *          - CW(顺时针): 状态按 1→3→2→6→4→5 顺序变化
 *          - CCW(逆时针): 状态按 1→5→4→6→2→3 顺序变化
 */

#ifndef __BSP_HALL_H
#define __BSP_HALL_H

#ifdef __cplusplus
extern "C" {
#endif

#include "main.h"

/* 方向状态常量 - 用于表示电机旋转方向 */
#define BSP_HALL_DIR_UNKNOWN   (0)   /**< @brief 方向未知或无效 */
#define BSP_HALL_DIR_CW        (1)   /**< @brief 顺时针方向 (Clockwise) */
#define BSP_HALL_DIR_CCW       (-1)  /**< @brief 逆时针方向 (Counter-Clockwise) */

/**
 * @brief 初始化霍尔传感器模块
 * @return uint8_t 0:成功, 其他:错误码
 * @note 启动TIM5定时器用于精确计时，采样初始霍尔状态
 */
uint8_t bsp_hall_init(void);

/**
 * @brief 获取当前霍尔状态
 * @return uint8_t 霍尔状态编码 (0-7)
 * @note 返回值为W:V:U格式的二进制编码
 */
uint8_t bsp_hall_get_state(void);

/**
 * @brief 获取上一次有效的霍尔状态
 * @return uint8_t 上一次的霍尔状态编码
 */
uint8_t bsp_hall_get_last_state(void);

/**
 * @brief 获取电机当前转速
 * @return uint32_t 转速 (RPM)
 * @note 基于两次换相时间间隔计算
 */
uint32_t bsp_hall_get_speed(void);

/**
 * @brief 获取有符号转速
 * @return int32_t 转速 (RPM), CW为正, CCW为负, 未知方向返回0
 */
int32_t bsp_hall_get_speed_signed(void);

/**
 * @brief 获取最近一次换相的时刻（定时器计数）
 * @return uint32_t 定时器计数值
 */
uint32_t bsp_hall_get_commutation_time(void);

/**
 * @brief 获取最近一次换相的周期（定时器滴答数）
 * @return uint32_t 周期（滴答数）
 */
uint32_t bsp_hall_get_period_ticks(void);

/**
 * @brief 获取最近一次换相的周期（微秒）
 * @return uint32_t 周期（微秒）
 */
uint32_t bsp_hall_get_period_us(void);

/**
 * @brief 获取霍尔边沿跳变计数
 * @return uint32_t 边沿计数
 */
uint32_t bsp_hall_get_edge_count(void);

/**
 * @brief 获取有效换相次数
 * @return uint32_t 有效转换计数
 */
uint32_t bsp_hall_get_valid_transition_count(void);

/**
 * @brief 获取无效换相次数
 * @return uint32_t 无效转换计数
 */
uint32_t bsp_hall_get_invalid_transition_count(void);

/**
 * @brief 获取当前旋转方向
 * @return int8_t 方向: CW(1), CCW(-1), UNKNOWN(0)
 */
int8_t bsp_hall_get_direction(void);

/**
 * @brief 检查EXTI中断是否使能
 * @return uint8_t 1:使能, 0:禁用
 * @note 当前架构使用轮询，此函数保留用于兼容
 */
uint8_t bsp_hall_is_irq_enabled(void);

/**
 * @brief 清除所有统计数据
 * @note 重置计数器和状态，但不停止定时器
 */
void bsp_hall_clear_stats(void);

/**
 * @brief 使能霍尔传感器EXTI中断
 * @note 当前版本已禁用EXTI，改用TIM1更新中断轮询
 */
void bsp_hall_irq_enable(void);

/**
 * @brief 禁用霍尔传感器EXTI中断
 * @note 当前版本已禁用EXTI，改用TIM1更新中断轮询
 */
void bsp_hall_irq_disable(void);

/**
 * @brief 轮询霍尔状态并执行换相
 * @note 在TIM1更新中断中调用，检测霍尔状态变化并触发motor_sensor_mode_phase()
 */
void bsp_hall_poll_and_commutate(void);

#ifdef __cplusplus
}
#endif

#endif /* __BSP_HALL_H */
