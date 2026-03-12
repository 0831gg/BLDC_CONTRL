#include "bsp_ctrl_sd.h"

void bsp_ctrl_sd_init(void)
{
    bsp_ctrl_sd_disable();
}

void bsp_ctrl_sd_enable(void)
{
    HAL_GPIO_WritePin(PWM_Enable_GPIO_Port, PWM_Enable_Pin, GPIO_PIN_SET);
}

void bsp_ctrl_sd_disable(void)
{
    HAL_GPIO_WritePin(PWM_Enable_GPIO_Port, PWM_Enable_Pin, GPIO_PIN_RESET);
}

uint8_t bsp_ctrl_sd_is_enabled(void)
{
    return (HAL_GPIO_ReadPin(PWM_Enable_GPIO_Port, PWM_Enable_Pin) == GPIO_PIN_SET) ? 1U : 0U;
}
