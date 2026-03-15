#include "bsp_pwm.h"
#include "bsp_ctrl_sd.h"

static uint16_t s_pwm_duty = 0U;
static uint8_t s_break_fault = 0U;

static uint16_t bsp_pwm_limit_duty(uint16_t duty)
{
    return (duty > BSP_PWM_PERIOD) ? BSP_PWM_PERIOD : duty;
}

static void bsp_pwm_disable_all_channels(void)
{
    TIM1->CCER &= (uint16_t)~(TIM_CCER_CC1E | TIM_CCER_CC2E | TIM_CCER_CC3E);
}

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

void bsp_pwm_duty_set(uint16_t duty)
{
    s_pwm_duty = bsp_pwm_limit_duty(duty);

    __HAL_TIM_SET_COMPARE(&htim1, TIM_CHANNEL_1, s_pwm_duty);
    __HAL_TIM_SET_COMPARE(&htim1, TIM_CHANNEL_2, s_pwm_duty);
    __HAL_TIM_SET_COMPARE(&htim1, TIM_CHANNEL_3, s_pwm_duty);
}

uint16_t bsp_pwm_duty_get(void)
{
    return s_pwm_duty;
}

void bsp_pwm_all_open(void)
{
    /* 阶段2：PWM已在init中启动，只需确保MOE使能 */
    __HAL_TIM_MOE_ENABLE(&htim1);
}

void bsp_pwm_all_close(void)
{
    /* 阶段2：不停止PWM，只禁用通道输出 */
    bsp_pwm_disable_all_channels();
}

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

void bsp_pwm_lower_set(uint8_t un, uint8_t vn, uint8_t wn)
{
    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_13, un ? GPIO_PIN_SET : GPIO_PIN_RESET);
    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_14, vn ? GPIO_PIN_SET : GPIO_PIN_RESET);
    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_15, wn ? GPIO_PIN_SET : GPIO_PIN_RESET);
}

void bsp_pwm_lower_all_off(void)
{
    bsp_pwm_lower_set(0U, 0U, 0U);
}

uint8_t bsp_pwm_is_break_fault(void)
{
    return s_break_fault;
}

void bsp_pwm_clear_break_fault(void)
{
    s_break_fault = 0U;
}

void bsp_pwm_disable_break(void)
{
    __HAL_TIM_DISABLE_IT(&htim1, TIM_IT_BREAK);
    TIM1->BDTR &= ~TIM_BDTR_BKE;
    __HAL_TIM_MOE_ENABLE(&htim1);
}

void HAL_TIMEx_BreakCallback(TIM_HandleTypeDef *htim)
{
    if (htim->Instance == TIM1) {
        s_break_fault = 1U;
        bsp_pwm_lower_all_off();
        bsp_pwm_all_close();
        bsp_ctrl_sd_disable();
    }
}
