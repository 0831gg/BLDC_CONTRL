#include "test_phase2.h"

#include "bsp_adc_motor.h"
#include "bsp_ctrl_sd.h"
#include "bsp_key.h"
#include "bsp_led.h"
#include "bsp_pwm.h"
#include "bsp_uart.h"
#include "tim.h"

#include <stdlib.h>

#define TEST_PHASE2_LED_INTERVAL_MS      (200U)
#define TEST_PHASE2_KEY_INTERVAL_MS      (20U)
#define TEST_PHASE2_PRINT_INTERVAL_MS    (500U)
#define TEST_PHASE2_ERROR_ADC1_CAL       (1U)
#define TEST_PHASE2_ERROR_ADC2_CAL       (2U)
#define TEST_PHASE2_ERROR_ADC1_DMA       (3U)
#define TEST_PHASE2_ERROR_ADC2_INJ       (4U)

static KeyGroup_HandleTypeDef s_key;
static uint32_t s_led_tick = 0U;
static uint32_t s_key_tick = 0U;
static uint32_t s_print_tick = 0U;
static uint8_t s_pwm_running = 0U;
static uint8_t s_adc_init_status = 0U;

static const char *test_phase2_get_adc1_mode_text(void)
{
    if (bsp_adc_motor_get_adc1_mode() == BSP_ADC1_MODE_POLLING) {
        return "polling_backup";
    }

    return "dma";
}

static void test_phase2_print_adc_error(uint8_t error_code)
{
    BSP_UART_Printf("[P2] ADC init failed, code=%u\r\n", (unsigned int)error_code);

    if (error_code == TEST_PHASE2_ERROR_ADC1_CAL) {
        BSP_UART_Printf("[P2] ADC1 calibration failed\r\n");
    } else if (error_code == TEST_PHASE2_ERROR_ADC2_CAL) {
        BSP_UART_Printf("[P2] ADC2 calibration failed\r\n");
    } else if (error_code == TEST_PHASE2_ERROR_ADC1_DMA) {
        BSP_UART_Printf("[P2] ADC1 DMA start failed\r\n");
    } else if (error_code == TEST_PHASE2_ERROR_ADC2_INJ) {
        BSP_UART_Printf("[P2] ADC2 injected start failed\r\n");
    }
}

static void test_phase2_apply_pwm_state(uint8_t enable_pwm)
{
    bsp_pwm_lower_all_off();
    bsp_pwm_all_close();

    if (enable_pwm != 0U) {
        bsp_ctrl_sd_enable();
        bsp_pwm_duty_set(BSP_PWM_DUTY_50_PERCENT);
        __HAL_TIM_SET_COMPARE(&htim1, TIM_CHANNEL_4, BSP_PWM_PERIOD / 2U);
        if (HAL_TIM_PWM_Start(&htim1, TIM_CHANNEL_4) != HAL_OK) {
            Error_Handler();
        }
        bsp_pwm_all_open();
        LED0_ON();
        LED1_OFF();
        s_pwm_running = 1U;
        BSP_UART_Printf("[P2] PWM trigger running: PA8/PA9/PA10 50%%, ADC2 by TIM1_CC4, CCR4=%u\r\n",
                        (unsigned int)(BSP_PWM_PERIOD / 2U));
    } else {
        bsp_ctrl_sd_disable();
        bsp_pwm_duty_set(0U);
        HAL_TIM_PWM_Stop(&htim1, TIM_CHANNEL_4);
        LED0_OFF();
        LED1_ON();
        s_pwm_running = 0U;
        BSP_UART_Printf("[P2] PWM trigger stopped\r\n");
    }
}

void Test_Phase2_Init(void)
{
    BSP_UART_Init();
    BSP_UART_Printf("\r\n[P2] Boot\r\n");

    LED_Init();
    Key_Init(&s_key, KEY_DEFAULT_LONG_PRESS_TIME);
    bsp_ctrl_sd_init();
    bsp_pwm_init();

    s_adc_init_status = bsp_adc_motor_init();
    if (s_adc_init_status != 0U) {
        LED0_ON();
        LED1_ON();
        test_phase2_print_adc_error(s_adc_init_status);
        return;
    }

    BSP_UART_Printf("\r\n[P2] Phase2 start\r\n");
    BSP_UART_Printf("[P2] ADC1 mode=%s\r\n", test_phase2_get_adc1_mode_text());
    BSP_UART_Printf("[P2] KEY0 toggle trigger, KEY2 stop\r\n");

    test_phase2_apply_pwm_state(1U);
}

void Test_Phase2_Loop(void)
{
    uint32_t now = HAL_GetTick();

    if (s_adc_init_status != 0U) {
        return;
    }

    if ((now - s_led_tick) >= TEST_PHASE2_LED_INTERVAL_MS) {
        s_led_tick = now;
        if (s_pwm_running != 0U) {
            LED0_TOGGLE();
        } else {
            LED1_TOGGLE();
        }
    }

    if ((now - s_key_tick) >= TEST_PHASE2_KEY_INTERVAL_MS) {
        s_key_tick = now;
        Key_Scan(&s_key);

        if (Key_GetEvent(&s_key, 0U) == KEY_EVENT_SHORT_PRESS) {
            test_phase2_apply_pwm_state((uint8_t)(s_pwm_running == 0U));
        }

        if (Key_GetEvent(&s_key, 2U) == KEY_EVENT_SHORT_PRESS) {
            test_phase2_apply_pwm_state(0U);
        }
    }

    if ((now - s_print_tick) >= TEST_PHASE2_PRINT_INTERVAL_MS) {
        int16_t iu = 0;
        int16_t iv = 0;
        int16_t iw = 0;
        uint32_t vbus_mv = 0U;
        int32_t temp_centi = 0;

        s_print_tick = now;
        bsp_adc_get_phase_current(&iu, &iv, &iw);
        vbus_mv = (uint32_t)(bsp_adc_get_vbus() * 1000.0f);
        temp_centi = (int32_t)(bsp_adc_get_temp() * 100.0f);

        BSP_UART_Printf("[P2] PWM=%u VBUS=(%lu.%03luV) TEMP=(%ld.%02ldC) IU=%d IV=%d IW=%d Jupd=%lu Break=%u\r\n",
                        (unsigned int)s_pwm_running,
                        (unsigned long)(vbus_mv / 1000U),
                        (unsigned long)(vbus_mv % 1000U),
                        (long)(temp_centi / 100),
                        (long)labs(temp_centi % 100),
                        (int)iu,
                        (int)iv,
                        (int)iw,
                        (unsigned long)bsp_adc_get_injected_update_count(),
                        (unsigned int)bsp_pwm_is_break_fault());

        if (bsp_pwm_is_break_fault() != 0U) {
            BSP_UART_Printf("[P2] Break fault detected\r\n");
            bsp_pwm_clear_break_fault();
        }
    }
}
