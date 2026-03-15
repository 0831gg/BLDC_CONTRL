#include "test_phase5.h"

#include "bsp_adc_motor.h"
#include "bsp_ctrl_sd.h"
#include "bsp_delay.h"
#include "bsp_hall.h"
#include "bsp_key.h"
#include "bsp_led.h"
#include "bsp_pwm.h"
#include "bsp_uart.h"
#include "motor_ctrl.h"
#include "tim.h"

#define TEST_PHASE5_LED_INTERVAL_MS       (200U)
#define TEST_PHASE5_KEY_INTERVAL_MS       (20U)
#define TEST_PHASE5_PRINT_INTERVAL_MS     (800U)
#define TEST_PHASE5_TRIGGER_CCR4          (BSP_PWM_PERIOD / 2U)

#define TEST_PHASE5_DUTY_1                ((BSP_PWM_PERIOD + 1U) * 5U / 100U)
#define TEST_PHASE5_DUTY_2                ((BSP_PWM_PERIOD + 1U) * 10U / 100U)
#define TEST_PHASE5_DUTY_3                ((BSP_PWM_PERIOD + 1U) * 15U / 100U)
#define TEST_PHASE5_DUTY_4                ((BSP_PWM_PERIOD + 1U) * 20U / 100U)

static const uint16_t s_duty_table[] = {
    TEST_PHASE5_DUTY_1,
    TEST_PHASE5_DUTY_2,
    TEST_PHASE5_DUTY_3,
    TEST_PHASE5_DUTY_4
};

static KeyGroup_HandleTypeDef s_key;
static uint32_t s_led_tick = 0U;
static uint32_t s_key_tick = 0U;
static uint32_t s_print_tick = 0U;
static uint8_t s_hall_init_status = 0U;
static uint8_t s_adc_init_status = 0U;
static uint8_t s_trigger_status = 0U;
static uint8_t s_duty_index = 0U;

static void test_phase5_print_reset_flags(void)
{
    BSP_UART_Printf("[P5] Reset flags: PIN=%u BOR=%u SFTRST=%u IWDG=%u WWDG=%u LPWR=%u OBL=%u\r\n",
                    (unsigned int)(__HAL_RCC_GET_FLAG(RCC_FLAG_PINRST) != RESET),
                    (unsigned int)(__HAL_RCC_GET_FLAG(RCC_FLAG_BORRST) != RESET),
                    (unsigned int)(__HAL_RCC_GET_FLAG(RCC_FLAG_SFTRST) != RESET),
                    (unsigned int)(__HAL_RCC_GET_FLAG(RCC_FLAG_IWDGRST) != RESET),
                    (unsigned int)(__HAL_RCC_GET_FLAG(RCC_FLAG_WWDGRST) != RESET),
                    (unsigned int)(__HAL_RCC_GET_FLAG(RCC_FLAG_LPWRRST) != RESET),
                    (unsigned int)(__HAL_RCC_GET_FLAG(RCC_FLAG_OBLRST) != RESET));
    __HAL_RCC_CLEAR_RESET_FLAGS();
}

static const char *test_phase5_dir_text(uint8_t direction)
{
    return (direction == MOTOR_DIR_CCW) ? "CCW" : "CW";
}

static const char *test_phase5_adc1_mode_text(void)
{
    if (bsp_adc_motor_get_adc1_mode() == BSP_ADC1_MODE_POLLING) {
        return "polling_backup";
    }

    return "dma";
}

static uint8_t test_phase5_is_ready(void)
{
    return (uint8_t)((s_hall_init_status == 0U) &&
                     (s_adc_init_status == 0U) &&
                     (s_trigger_status == 0U));
}

static void test_phase5_refresh_led(void)
{
    if (motor_ctrl_is_running() != 0U) {
        LED1_OFF();
        LED0_TOGGLE();
    } else {
        LED0_OFF();
        LED1_TOGGLE();
    }
}

static uint8_t test_phase5_start_adc_trigger(void)
{
    __HAL_TIM_SET_COMPARE(&htim1, TIM_CHANNEL_4, TEST_PHASE5_TRIGGER_CCR4);

    if (HAL_TIM_PWM_Start(&htim1, TIM_CHANNEL_4) != HAL_OK) {
        return 1U;
    }

    return 0U;
}

static void test_phase5_print_assist_trace(void)
{
    uint8_t state = motor_ctrl_get_startup_hall_state();
    uint16_t duty = motor_ctrl_get_startup_duty();
    uint16_t duty_percent = (duty * 100U) / (BSP_PWM_PERIOD + 1U);

    BSP_UART_Printf("[P5] Assist stage=%s step=%u hall=%u%u%u action=%s duty=%u%%\r\n",
                    motor_ctrl_get_startup_stage_text(),
                    (unsigned int)motor_ctrl_get_startup_step(),
                    (unsigned int)((state >> 2) & 0x01U),
                    (unsigned int)((state >> 1) & 0x01U),
                    (unsigned int)(state & 0x01U),
                    motor_phase_action_name(motor_ctrl_get_startup_action()),
                    (unsigned int)duty_percent);
}

static void test_phase5_start_motor(void)
{
    uint16_t duty_percent = (s_duty_table[s_duty_index] * 100U) / (BSP_PWM_PERIOD + 1U);

    BSP_UART_Printf("[P5] Start request dir=%s duty=%u%% off=%u hall=%u%u%u\r\n",
                    test_phase5_dir_text(motor_ctrl_get_direction()),
                    (unsigned int)duty_percent,
                    (unsigned int)motor_phase_tab_get_commutation_offset(),
                    (unsigned int)((bsp_hall_get_state() >> 2) & 0x01U),
                    (unsigned int)((bsp_hall_get_state() >> 1) & 0x01U),
                    (unsigned int)(bsp_hall_get_state() & 0x01U));

    if (motor_start_simple(s_duty_table[s_duty_index], motor_ctrl_get_direction()) == 0) {
        uint16_t motor_duty_percent = (motor_ctrl_get_duty() * 100U) / (BSP_PWM_PERIOD + 1U);
        test_phase5_print_assist_trace();
        BSP_UART_Printf("[P5] Motor start dir=%s duty=%u%% off=%u action=%s sd=%u\r\n",
                        test_phase5_dir_text(motor_ctrl_get_direction()),
                        (unsigned int)motor_duty_percent,
                        (unsigned int)motor_phase_tab_get_commutation_offset(),
                        motor_phase_action_name(motor_ctrl_get_last_action()),
                        (unsigned int)bsp_ctrl_sd_is_enabled());
    } else {
        test_phase5_print_assist_trace();
        BSP_UART_Printf("[P5] Motor start failed off=%u fault=%u hall=%u%u%u break=%u\r\n",
                        (unsigned int)motor_phase_tab_get_commutation_offset(),
                        (unsigned int)motor_ctrl_get_fault(),
                        (unsigned int)((bsp_hall_get_state() >> 2) & 0x01U),
                        (unsigned int)((bsp_hall_get_state() >> 1) & 0x01U),
                        (unsigned int)(bsp_hall_get_state() & 0x01U),
                        (unsigned int)bsp_pwm_is_break_fault());
    }
}

static void test_phase5_print_status(void)
{
    uint8_t state = bsp_hall_get_state();
    uint32_t vbus_mv = 0U;
    uint16_t duty_percent = (s_duty_table[s_duty_index] * 100U) / (BSP_PWM_PERIOD + 1U);

    vbus_mv = (uint32_t)(bsp_adc_get_vbus() * 1000.0f);

    BSP_UART_Printf("[P5] Run=%u Dir=%s Off=%u Hall=%u%u%u Duty=%u%% Act=%s Fault=%u Edge=%lu Valid=%lu Invalid=%lu RPM=%lu Break=%u VBUS=%lu.%03luV\r\n",
                    (unsigned int)motor_ctrl_is_running(),
                    test_phase5_dir_text(motor_ctrl_get_direction()),
                    (unsigned int)motor_phase_tab_get_commutation_offset(),
                    (unsigned int)((state >> 2) & 0x01U),
                    (unsigned int)((state >> 1) & 0x01U),
                    (unsigned int)(state & 0x01U),
                    (unsigned int)duty_percent,
                    motor_phase_action_name(motor_ctrl_get_last_action()),
                    (unsigned int)motor_ctrl_get_fault(),
                    (unsigned long)bsp_hall_get_edge_count(),
                    (unsigned long)bsp_hall_get_valid_transition_count(),
                    (unsigned long)bsp_hall_get_invalid_transition_count(),
                    (unsigned long)bsp_hall_get_speed(),
                    (unsigned int)bsp_pwm_is_break_fault(),
                    (unsigned long)(vbus_mv / 1000U),
                    (unsigned long)(vbus_mv % 1000U));
}

void Test_Phase5_Init(void)
{
    Delay_Init();
    BSP_UART_Init();
    BSP_UART_Printf("\r\n[P5] Boot\r\n");
    test_phase5_print_reset_flags();

    LED_Init();
    Key_Init(&s_key, KEY_DEFAULT_LONG_PRESS_TIME);
    bsp_ctrl_sd_init();
    bsp_pwm_init();

    s_hall_init_status = bsp_hall_init();
    if (s_hall_init_status != 0U) {
        LED0_ON();
        LED1_ON();
        BSP_UART_Printf("[P5] Hall init failed, code=%u\r\n", (unsigned int)s_hall_init_status);
        return;
    }

    s_adc_init_status = bsp_adc_motor_init();
    if (s_adc_init_status != 0U) {
        LED0_ON();
        LED1_ON();
        BSP_UART_Printf("[P5] ADC init failed, code=%u\r\n", (unsigned int)s_adc_init_status);
        return;
    }

    s_trigger_status = test_phase5_start_adc_trigger();
    if (s_trigger_status != 0U) {
        LED0_ON();
        LED1_ON();
        BSP_UART_Printf("[P5] TIM1 CH4 trigger start failed\r\n");
        return;
    }

    motor_ctrl_init();

    BSP_UART_Printf("\r\n[P5] Phase5 start\r\n");
    BSP_UART_Printf("[P5] BKIN protection DISABLED for testing\r\n");
    bsp_pwm_disable_break();
    BSP_UART_Printf("[P5] Integrated low-voltage only: driver+motor connected, current-limited bus\r\n");
    BSP_UART_Printf("[P5] KEY0 start/stop, KEY1 dir toggle, KEY2 short=duty, KEY2 long=offset\r\n");
    BSP_UART_Printf("[P5] Start assist: align + open-loop kick + wait first valid Hall edge\r\n");
    BSP_UART_Printf("[P5] ADC1=%s ADC2_trigger=TIM1_OC4REF CCR4=%u\r\n",
                    test_phase5_adc1_mode_text(),
                    (unsigned int)TEST_PHASE5_TRIGGER_CCR4);
    BSP_UART_Printf("[P5] Hall order=UVW, PA0=U PA1=V PA2=W\r\n");
    BSP_UART_Printf("[P5] Offset=%u\r\n", (unsigned int)motor_phase_tab_get_commutation_offset());
    test_phase5_print_status();
}

void Test_Phase5_Loop(void)
{
    uint32_t now = HAL_GetTick();
    KeyEvent_TypeDef key0_event = KEY_EVENT_NONE;
    KeyEvent_TypeDef key1_event = KEY_EVENT_NONE;
    KeyEvent_TypeDef key2_event = KEY_EVENT_NONE;

    if (test_phase5_is_ready() == 0U) {
        return;
    }

    if ((now - s_led_tick) >= TEST_PHASE5_LED_INTERVAL_MS) {
        s_led_tick = now;
        test_phase5_refresh_led();
    }

    if ((now - s_key_tick) >= TEST_PHASE5_KEY_INTERVAL_MS) {
        s_key_tick = now;
        Key_Scan(&s_key);
        key0_event = Key_GetEvent(&s_key, 0U);
        key1_event = Key_GetEvent(&s_key, 1U);
        key2_event = Key_GetEvent(&s_key, 2U);

        if (key0_event == KEY_EVENT_SHORT_PRESS) {
            if (motor_ctrl_is_running() != 0U) {
                motor_stop();
                BSP_UART_Printf("[P5] Motor stop\r\n");
            } else {
                test_phase5_start_motor();
            }
        }

        if (key1_event == KEY_EVENT_SHORT_PRESS) {
            uint8_t next_dir = (motor_ctrl_get_direction() == MOTOR_DIR_CCW) ? MOTOR_DIR_CW : MOTOR_DIR_CCW;

            motor_ctrl_set_direction(next_dir);
            BSP_UART_Printf("[P5] Direction=%s\r\n", test_phase5_dir_text(next_dir));

            if (motor_ctrl_is_running() != 0U) {
                motor_stop();
                test_phase5_start_motor();
            }
        }

        if (key2_event == KEY_EVENT_SHORT_PRESS) {
            s_duty_index = (uint8_t)((s_duty_index + 1U) % (sizeof(s_duty_table) / sizeof(s_duty_table[0])));
            motor_ctrl_set_duty(s_duty_table[s_duty_index]);
            BSP_UART_Printf("[P5] Duty=%u\r\n", (unsigned int)s_duty_table[s_duty_index]);
        }

        if (key2_event == KEY_EVENT_LONG_PRESS) {
            uint8_t next_offset = (uint8_t)((motor_phase_tab_get_commutation_offset() + 1U) % 6U);

            motor_phase_tab_set_commutation_offset(next_offset);
            BSP_UART_Printf("[P5] Offset=%u\r\n", (unsigned int)next_offset);

            if (motor_ctrl_is_running() != 0U) {
                motor_stop();
                test_phase5_start_motor();
            }
        }
    }

    if (bsp_pwm_is_break_fault() != 0U) {
        motor_ctrl_set_fault(MOTOR_FAULT_BREAK);
        motor_stop();
        BSP_UART_Printf("[P5] Break fault detected\r\n");
        bsp_pwm_clear_break_fault();
    }

    if ((motor_ctrl_is_running() != 0U) &&
        (motor_ctrl_get_fault() == MOTOR_FAULT_HALL_INVALID)) {
        motor_stop();
        BSP_UART_Printf("[P5] Hall invalid stop\r\n");
    }

    if ((now - s_print_tick) >= TEST_PHASE5_PRINT_INTERVAL_MS) {
        s_print_tick = now;
        test_phase5_print_status();
    }
}
