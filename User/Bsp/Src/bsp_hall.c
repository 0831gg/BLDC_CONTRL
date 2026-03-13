#include "bsp_hall.h"

#include "tim.h"

#define BSP_HALL_TIM5_TICK_HZ              (50000UL)
#define BSP_HALL_TRANSITIONS_PER_REV       (6UL)
#define BSP_HALL_ERROR_TIM5_START          (1U)

static volatile uint8_t s_hall_state = 0U;
static volatile uint8_t s_hall_last_state = 0U;
static volatile uint32_t s_last_commutation_time = 0U;
static volatile uint32_t s_last_period_ticks = 0U;
static volatile uint32_t s_edge_count = 0U;
static volatile uint32_t s_valid_transition_count = 0U;
static volatile uint32_t s_invalid_transition_count = 0U;
static volatile int8_t s_direction = BSP_HALL_DIR_UNKNOWN;
static volatile uint8_t s_irq_enabled = 0U;

static uint8_t bsp_hall_sample_state(void)
{
    uint8_t state = 0U;

    if (HAL_GPIO_ReadPin(HALL_U_GPIO_Port, HALL_U_Pin) == GPIO_PIN_SET) {
        state |= 0x04U;
    }

    if (HAL_GPIO_ReadPin(HALL_V_GPIO_Port, HALL_V_Pin) == GPIO_PIN_SET) {
        state |= 0x02U;
    }

    if (HAL_GPIO_ReadPin(HALL_W_GPIO_Port, HALL_W_Pin) == GPIO_PIN_SET) {
        state |= 0x01U;
    }

    return state;
}

static uint8_t bsp_hall_is_valid_state(uint8_t state)
{
    return (uint8_t)((state == 0x01U) ||
                     (state == 0x05U) ||
                     (state == 0x04U) ||
                     (state == 0x06U) ||
                     (state == 0x02U) ||
                     (state == 0x03U));
}

static uint8_t bsp_hall_get_next_state(uint8_t state)
{
    switch (state) {
        case 0x01U: return 0x05U;
        case 0x05U: return 0x04U;
        case 0x04U: return 0x06U;
        case 0x06U: return 0x02U;
        case 0x02U: return 0x03U;
        case 0x03U: return 0x01U;
        default:    return 0x00U;
    }
}

static uint8_t bsp_hall_get_prev_state(uint8_t state)
{
    switch (state) {
        case 0x01U: return 0x03U;
        case 0x05U: return 0x01U;
        case 0x04U: return 0x05U;
        case 0x06U: return 0x04U;
        case 0x02U: return 0x06U;
        case 0x03U: return 0x02U;
        default:    return 0x00U;
    }
}

uint8_t bsp_hall_init(void)
{
    uint32_t now;
    uint8_t state;

    if (HAL_TIM_Base_Start(&htim5) != HAL_OK) {
        return BSP_HALL_ERROR_TIM5_START;
    }

    state = bsp_hall_sample_state();
    now = __HAL_TIM_GET_COUNTER(&htim5);

    s_hall_state = state;
    s_hall_last_state = state;
    s_last_commutation_time = now;
    s_last_period_ticks = 0U;
    s_edge_count = 0U;
    s_valid_transition_count = 0U;
    s_invalid_transition_count = 0U;
    s_direction = BSP_HALL_DIR_UNKNOWN;

    bsp_hall_irq_enable();
    return 0U;
}

uint8_t bsp_hall_get_state(void)
{
    return s_hall_state;
}

uint8_t bsp_hall_get_last_state(void)
{
    return s_hall_last_state;
}

uint32_t bsp_hall_get_speed(void)
{
    uint32_t ticks = s_last_period_ticks;

    if (ticks == 0U) {
        return 0U;
    }

    return (uint32_t)((BSP_HALL_TIM5_TICK_HZ * 60UL) / (ticks * BSP_HALL_TRANSITIONS_PER_REV));
}

uint32_t bsp_hall_get_commutation_time(void)
{
    return s_last_commutation_time;
}

uint32_t bsp_hall_get_period_ticks(void)
{
    return s_last_period_ticks;
}

uint32_t bsp_hall_get_period_us(void)
{
    return (uint32_t)((s_last_period_ticks * 1000000ULL) / BSP_HALL_TIM5_TICK_HZ);
}

uint32_t bsp_hall_get_edge_count(void)
{
    return s_edge_count;
}

uint32_t bsp_hall_get_valid_transition_count(void)
{
    return s_valid_transition_count;
}

uint32_t bsp_hall_get_invalid_transition_count(void)
{
    return s_invalid_transition_count;
}

int8_t bsp_hall_get_direction(void)
{
    return s_direction;
}

uint8_t bsp_hall_is_irq_enabled(void)
{
    return s_irq_enabled;
}

void bsp_hall_clear_stats(void)
{
    uint32_t now = __HAL_TIM_GET_COUNTER(&htim5);
    uint8_t state = bsp_hall_sample_state();

    s_hall_state = state;
    s_hall_last_state = state;
    s_last_commutation_time = now;
    s_last_period_ticks = 0U;
    s_edge_count = 0U;
    s_valid_transition_count = 0U;
    s_invalid_transition_count = 0U;
    s_direction = BSP_HALL_DIR_UNKNOWN;
}

void bsp_hall_irq_enable(void)
{
    HAL_NVIC_EnableIRQ(HALL_U_EXTI_IRQn);
    HAL_NVIC_EnableIRQ(HALL_V_EXTI_IRQn);
    HAL_NVIC_EnableIRQ(HALL_W_EXTI_IRQn);
    s_irq_enabled = 1U;
}

void bsp_hall_irq_disable(void)
{
    HAL_NVIC_DisableIRQ(HALL_U_EXTI_IRQn);
    HAL_NVIC_DisableIRQ(HALL_V_EXTI_IRQn);
    HAL_NVIC_DisableIRQ(HALL_W_EXTI_IRQn);
    s_irq_enabled = 0U;
}

void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin)
{
    uint8_t new_state;
    uint8_t old_state;
    uint32_t now;

    if ((GPIO_Pin != HALL_U_Pin) &&
        (GPIO_Pin != HALL_V_Pin) &&
        (GPIO_Pin != HALL_W_Pin)) {
        return;
    }

    new_state = bsp_hall_sample_state();
    old_state = s_hall_state;

    if (new_state == old_state) {
        return;
    }

    s_edge_count++;
    s_hall_last_state = old_state;
    s_hall_state = new_state;

    if (bsp_hall_is_valid_state(old_state) && bsp_hall_is_valid_state(new_state)) {
        if (bsp_hall_get_next_state(old_state) == new_state) {
            s_direction = BSP_HALL_DIR_CCW;
        } else if (bsp_hall_get_prev_state(old_state) == new_state) {
            s_direction = BSP_HALL_DIR_CW;
        } else {
            s_direction = BSP_HALL_DIR_UNKNOWN;
            s_invalid_transition_count++;
            return;
        }

        now = __HAL_TIM_GET_COUNTER(&htim5);
        s_last_period_ticks = now - s_last_commutation_time;
        s_last_commutation_time = now;
        s_valid_transition_count++;
    } else {
        s_direction = BSP_HALL_DIR_UNKNOWN;
        s_invalid_transition_count++;
    }
}
