/**
 * @file    bsp_pwm.c
 * @brief   TIM1 PWM驱动实现
 * @details 实现TIM1三通道PWM输出控制，用于BLDC电机的调速和换相
 * @see     bsp_pwm.h
 */

#include "bsp_pwm.h"
#include "bsp_ctrl_sd.h"

/*============================================================================
 * 私有变量定义
 *============================================================================*/

/** @brief 当前PWM占空比值 */
static uint16_t s_pwm_duty = 0U;
/** @brief Break故障标志 */
static uint8_t s_break_fault = 0U;

/*============================================================================
 * 私有函数实现
 *============================================================================*/

/**
 * @brief 限制占空比在有效范围内
 * @param duty 原始占空比
 * @return uint16_t 限制后的占空比
 */
static uint16_t bsp_pwm_limit_duty(uint16_t duty)
{
    return (duty > BSP_PWM_PERIOD) ? BSP_PWM_PERIOD : duty;
}

/**
 * @brief 禁用所有PWM通道输出
 * @note 直接操作TIM1寄存器快速关闭输出
 */
static void bsp_pwm_disable_all_channels(void)
{
    TIM1->CCER &= (uint16_t)~(TIM_CCER_CC1E | TIM_CCER_CC2E | TIM_CCER_CC3E);
}

/*============================================================================
 * 公共函数实现
 *============================================================================*/

/**
 * @brief 初始化PWM模块
 * @note 1. 复位占空比和故障标志
 *       2. 设置初始PWM占空比为0
 *       3. 关闭所有下桥臂
 *       4. 启动三通道PWM输出
 *       5. 使能主输出(MOE)
 *       6. 启动TIM1更新中断用于霍尔轮询
 */
void bsp_pwm_init(void)
{
    s_break_fault = 0U;
    s_pwm_duty = 0U;

    bsp_pwm_duty_set(0U);
    bsp_pwm_lower_all_off();

    /* 阶段2：PWM始终开启 */
    HAL_TIM_PWM_Start(&htim1, TIM_CHANNEL_1);
    HAL_TIM_PWM_Start(&htim1, TIM_CHANNEL_2);
    HAL_TIM_PWM_Start(&htim1, TIM_CHANNEL_3);
    __HAL_TIM_MOE_ENABLE(&htim1);

    /* 阶段3：启动TIM1更新中断用于霍尔轮询 */
    HAL_TIM_Base_Start_IT(&htim1);
}

/**
 * @brief 设置PWM占空比
 * @param duty 占空比值 (0 ~ BSP_PWM_PERIOD)
 * @note 三个通道使用相同的占空比，由motor_ctrl模块控制各相通断
 */
void bsp_pwm_duty_set(uint16_t duty)
{
    s_pwm_duty = bsp_pwm_limit_duty(duty);

    /* 设置三通道的比较值 */
    __HAL_TIM_SET_COMPARE(&htim1, TIM_CHANNEL_1, s_pwm_duty);
    __HAL_TIM_SET_COMPARE(&htim1, TIM_CHANNEL_2, s_pwm_duty);
    __HAL_TIM_SET_COMPARE(&htim1, TIM_CHANNEL_3, s_pwm_duty);
}

/**
 * @brief 获取当前PWM占空比
 * @return uint16_t 占空比值
 */
uint16_t bsp_pwm_duty_get(void)
{
    return s_pwm_duty;
}

/**
 * @brief 打开所有PWM通道
 * @note 启动主输出使能，确保PWM可以正常输出
 */
void bsp_pwm_all_open(void)
{
    /* 阶段2：PWM已在init中启动，只需确保MOE使能 */
    __HAL_TIM_MOE_ENABLE(&htim1);
}

/**
 * @brief 关闭所有PWM通道
 * @note 禁用三通道输出但定时器继续运行，可快速停止PWM输出
 */
void bsp_pwm_all_close(void)
{
    /* 阶段2：不停止PWM，只禁用通道输出 */
    bsp_pwm_disable_all_channels();
}

/**
 * @brief 控制单相PWM输出（六步换相核心）
 * @param phase 要使能的相位
 * @note 这是BLDC六步换相的关键函数：
 *       - 禁用所有通道
 *       - 只使能指定相位的通道输出PWM
 *       - 其他相位的下桥臂由motor_ctrl控制
 */
void bsp_pwm_ctrl_single_phase(bsp_pwm_phase_t phase)
{
    bsp_pwm_disable_all_channels();

    switch (phase) {
        case BSP_PWM_PHASE_U:
            TIM1->CCER |= TIM_CCER_CC1E;
            break;
        case BSP_PWM_PHASE_V:
            TIM1->CCER |= TIM_CCER_CC2E;
            break;
        case BSP_PWM_PHASE_W:
            TIM1->CCER |= TIM_CCER_CC3E;
            break;
        case BSP_PWM_PHASE_NONE:
        default:
            break;
    }
}

/**
 * @brief 设置下桥臂GPIO状态
 * @param un U相下桥臂控制
 * @param vn V相下桥臂控制
 * @param wn W相下桥臂控制
 * @note 直接操作GPIO控制下桥臂 MOSFET/IGBT
 */
void bsp_pwm_lower_set(uint8_t un, uint8_t vn, uint8_t wn)
{
    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_13, un ? GPIO_PIN_SET : GPIO_PIN_RESET);
    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_14, vn ? GPIO_PIN_SET : GPIO_PIN_RESET);
    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_15, wn ? GPIO_PIN_SET : GPIO_PIN_RESET);
}

/**
 * @brief 关闭所有下桥臂
 * @note 紧急停止时调用，切断电机所有电流通路
 */
void bsp_pwm_lower_all_off(void)
{
    bsp_pwm_lower_set(0U, 0U, 0U);
}

/**
 * @brief 检查Break故障状态
 * @return uint8_t 1:故障, 0:正常
 */
uint8_t bsp_pwm_is_break_fault(void)
{
    return s_break_fault;
}

/**
 * @brief 清除Break故障标志
 */
void bsp_pwm_clear_break_fault(void)
{
    s_break_fault = 0U;
}

/**
 * @brief 禁用Break保护功能
 * @note 在调试或特殊情况下使用，正常运行时应保持Break使能
 */
void bsp_pwm_disable_break(void)
{
    __HAL_TIM_DISABLE_IT(&htim1, TIM_IT_BREAK);
    TIM1->BDTR &= ~TIM_BDTR_BKE;
    __HAL_TIM_MOE_ENABLE(&htim1);
}

/**
 * @brief TIM1 Break中断回调
 * @note 硬件故障检测到时自动调用，关闭所有输出以保护电路
 */
void HAL_TIMEx_BreakCallback(TIM_HandleTypeDef *htim)
{
    if (htim->Instance == TIM1) {
        s_break_fault = 1U;
        bsp_pwm_lower_all_off();    /* 关闭下桥臂 */
        bsp_pwm_all_close();         /* 关闭PWM通道 */
        bsp_ctrl_sd_disable();       /* 禁用驱动芯片 */
    }
}
