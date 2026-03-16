/**
 * @file    bsp_ctrl_sd.c
 * @brief   半桥驱动使能控制实现
 * @details 通过GPIO控制驱动芯片的SD引脚，实现安全可靠的驱动控制
 * @see     bsp_ctrl_sd.h
 */

#include "bsp_ctrl_sd.h"

/*============================================================================
 * 公共函数实现
 *============================================================================*/

/**
 * @brief 初始化驱动芯片控制
 * @note 默认禁用驱动芯片，确保系统安全启动
 */
void bsp_ctrl_sd_init(void)
{
    bsp_ctrl_sd_disable();
}

/**
 * @brief 使能驱动芯片
 * @note 拉高SD引脚，允许PWM信号输出到功率器件
 */
void bsp_ctrl_sd_enable(void)
{
    HAL_GPIO_WritePin(PWM_Enable_GPIO_Port, PWM_Enable_Pin, GPIO_PIN_SET);
}

/**
 * @brief 禁用驱动芯片
 * @note 拉低SD引脚，立即关闭所有功率器件输出
 *       这是最安全的紧急停止方式
 */
void bsp_ctrl_sd_disable(void)
{
    HAL_GPIO_WritePin(PWM_Enable_GPIO_Port, PWM_Enable_Pin, GPIO_PIN_RESET);
}

/**
 * @brief 检查驱动芯片是否使能
 * @return uint8_t 1:已使能, 0:已禁用
 */
uint8_t bsp_ctrl_sd_is_enabled(void)
{
    return (HAL_GPIO_ReadPin(PWM_Enable_GPIO_Port, PWM_Enable_Pin) == GPIO_PIN_SET) ? 1U : 0U;
}
