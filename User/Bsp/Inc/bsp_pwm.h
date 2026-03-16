/**
 * @file    bsp_pwm.h
 * @brief   TIM1 PWM输出与下桥臂控制
 * @details 本模块负责BLDC电机的PWM调速控制，使用TIM1的三通道PWM输出
 *          驱动三相H桥上桥臂，同时通过GPIO控制下桥臂的开关。
 *
 * @note    TIM1配置:
 *          - PWM频率: 84MHz / (8399+1) = 10kHz
 *          - 通道1: U相上桥臂 (PE9)
 *          - 通道2: V相上桥臂 (PE11)
 *          - 通道3: W相上桥臂 (PE13)
 *
 * @note    下桥臂控制引脚 (低侧驱动使能):
 *          - UN: PB13 (U相下桥臂)
 *          - VN: PB14 (V相下桥臂)
 *          - WN: PB15 (W相下桥臂)
 *
 * @note    六步换相控制:
 *          - 每次只有一相上桥臂输出PWM
 *          - 另一相下桥臂导通形成回路
 *          - 第三相高阻态
 */

#ifndef __BSP_PWM_H
#define __BSP_PWM_H

#ifdef __cplusplus
extern "C" {
#endif

#include "main.h"
#include "tim.h"

/** @brief PWM计数器周期值 (8399+1 = 8400) */
#define BSP_PWM_PERIOD              (8399U)
/** @brief 50%占空比对应的比较值 */
#define BSP_PWM_DUTY_50_PERCENT     (BSP_PWM_PERIOD / 2U)

/**
 * @brief PWM相位枚举
 * @note 用于指定当前控制的电机相位
 */
typedef enum {
    BSP_PWM_PHASE_NONE = 0,   /**< @brief 无相位（关闭所有通道） */
    BSP_PWM_PHASE_U,          /**< @brief U相 */
    BSP_PWM_PHASE_V,          /**< @brief V相 */
    BSP_PWM_PHASE_W           /**< @brief W相 */
} bsp_pwm_phase_t;

/**
 * @brief 初始化PWM模块
 * @note 配置TIM1三个通道为PWM输出模式，启动主输出使能(MOE)
 *       并启动TIM1更新中断用于霍尔传感器轮询
 */
void bsp_pwm_init(void);

/**
 * @brief 设置PWM占空比
 * @param duty 占空比值 (0 ~ BSP_PWM_PERIOD)
 * @note 三个通道同时设置相同的占空比
 */
void bsp_pwm_duty_set(uint16_t duty);

/**
 * @brief 获取当前PWM占空比
 * @return uint16_t 当前占空比值
 */
uint16_t bsp_pwm_duty_get(void);

/**
 * @brief 打开所有PWM通道
 * @note 启动TIM1主输出使能(MOE)，使能所有通道输出
 */
void bsp_pwm_all_open(void);

/**
 * @brief 关闭所有PWM通道
 * @note 禁用TIM1三个通道的输出，但定时器继续运行
 */
void bsp_pwm_all_close(void);

/**
 * @brief 控制单相PWM输出
 * @param phase 要使能的相位 (BSP_PWM_PHASE_U/V/W)
 * @note 只使能指定相位的上桥臂PWM输出，其他两相关闭
 *       这是六步换相中的关键操作
 */
void bsp_pwm_ctrl_single_phase(bsp_pwm_phase_t phase);

/**
 * @brief 设置下桥臂开关状态
 * @param un U相下桥臂: 0=关闭, 1=开启
 * @param vn V相下桥臂: 0=关闭, 1=开启
 * @param wn W相下桥臂: 0=关闭, 1=开启
 * @note 下桥臂由GPIO直接控制，用于形成电流通路
 */
void bsp_pwm_lower_set(uint8_t un, uint8_t vn, uint8_t wn);

/**
 * @brief 关闭所有下桥臂
 * @note 紧急停止或换相时调用，切断所有电流通路
 */
void bsp_pwm_lower_all_off(void);

/**
 * @brief 检查是否发生Break故障
 * @return uint8_t 1:发生故障, 0:正常
 * @note Break故障由硬件检测（过流、过压等）触发
 */
uint8_t bsp_pwm_is_break_fault(void);

/**
 * @brief 清除Break故障标志
 * @note 故障清除后需要重新使能PWM输出
 */
void bsp_pwm_clear_break_fault(void);

/**
 * @brief 禁用Break功能
 * @note 禁用TIM1的Break中断和硬件故障输入
 */
void bsp_pwm_disable_break(void);

#ifdef __cplusplus
}
#endif

#endif /* __BSP_PWM_H */
