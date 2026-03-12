#include "test_phase1.h"
#include "bsp_ctrl_sd.h"
#include "bsp_pwm.h"
#include "bsp_led.h"
#include "bsp_key.h"
#include "bsp_uart.h"

#define TEST_PHASE1_LED_INTERVAL_MS     (200U)
#define TEST_PHASE1_KEY_INTERVAL_MS     (20U)

typedef enum {
    TEST_PHASE1_STEP_DISABLE = 0,
    TEST_PHASE1_STEP_ENABLE,
    TEST_PHASE1_STEP_PWM_ALL,
    TEST_PHASE1_STEP_PWM_U_ONLY,
    TEST_PHASE1_STEP_LOWER_ON,
    TEST_PHASE1_STEP_ALL_OFF,
    TEST_PHASE1_STEP_MAX
} test_phase1_step_t;

static uint32_t s_led_tick = 0U;
static uint32_t s_key_tick = 0U;
static test_phase1_step_t s_step = TEST_PHASE1_STEP_DISABLE;
static KeyGroup_HandleTypeDef s_key;

static void test_phase1_show_step_led(test_phase1_step_t step)
{
    switch (step) {
        case TEST_PHASE1_STEP_DISABLE:
            LED0_OFF();
            LED1_OFF();
            break;

        case TEST_PHASE1_STEP_ENABLE:
            LED0_ON();
            LED1_OFF();
            break;

        case TEST_PHASE1_STEP_PWM_ALL:
            LED0_OFF();
            LED1_ON();
            break;

        case TEST_PHASE1_STEP_PWM_U_ONLY:
            LED0_ON();
            LED1_ON();
            break;

        case TEST_PHASE1_STEP_LOWER_ON:
            LED0_TOGGLE();
            LED1_OFF();
            break;

        case TEST_PHASE1_STEP_ALL_OFF:
        default:
            LED0_OFF();
            LED1_TOGGLE();
            break;
    }
}

static void test_phase1_apply_step(test_phase1_step_t step)
{
    bsp_pwm_lower_all_off();
    bsp_pwm_all_close();
    LED0_OFF();
    LED1_OFF();

    switch (step) {
        case TEST_PHASE1_STEP_DISABLE:
            bsp_ctrl_sd_disable();
            test_phase1_show_step_led(step);
            BSP_UART_Printf("[P1] Step0: SD disable, PWM off\r\n");
            break;

        case TEST_PHASE1_STEP_ENABLE:
            bsp_ctrl_sd_enable();
            test_phase1_show_step_led(step);
            BSP_UART_Printf("[P1] Step1: SD enable only\r\n");
            break;

        case TEST_PHASE1_STEP_PWM_ALL:
            bsp_ctrl_sd_enable();
            bsp_pwm_duty_set(BSP_PWM_DUTY_50_PERCENT);
            bsp_pwm_all_open();
            test_phase1_show_step_led(step);
            BSP_UART_Printf("[P1] Step2: CH1/2/3 50%% PWM\r\n");
            break;

        case TEST_PHASE1_STEP_PWM_U_ONLY:
            bsp_ctrl_sd_enable();
            bsp_pwm_duty_set(BSP_PWM_DUTY_50_PERCENT);
            bsp_pwm_all_open();
            bsp_pwm_ctrl_single_phase(BSP_PWM_PHASE_U);
            test_phase1_show_step_led(step);
            BSP_UART_Printf("[P1] Step3: U phase PWM only\r\n");
            break;

        case TEST_PHASE1_STEP_LOWER_ON:
            bsp_ctrl_sd_enable();
            bsp_pwm_lower_set(1U, 0U, 1U);
            test_phase1_show_step_led(step);
            BSP_UART_Printf("[P1] Step4: Lower UN/WN on\r\n");
            break;

        case TEST_PHASE1_STEP_ALL_OFF:
        default:
            bsp_ctrl_sd_disable();
            bsp_pwm_duty_set(0U);
            bsp_pwm_lower_all_off();
            test_phase1_show_step_led(step);
            BSP_UART_Printf("[P1] Step5: All outputs off\r\n");
            break;
    }

    BSP_UART_Printf("[P1] Step=%u SD=%u Duty=%u Break=%u\r\n",
                    (unsigned int)step,
                    (unsigned int)bsp_ctrl_sd_is_enabled(),
                    (unsigned int)bsp_pwm_duty_get(),
                    (unsigned int)bsp_pwm_is_break_fault());
}

static void test_phase1_step_next(void)
{
    s_step = (test_phase1_step_t)((s_step + 1U) % TEST_PHASE1_STEP_MAX);
    test_phase1_apply_step(s_step);
}

static void test_phase1_step_prev(void)
{
    if (s_step == TEST_PHASE1_STEP_DISABLE) {
        s_step = (test_phase1_step_t)(TEST_PHASE1_STEP_MAX - 1U);
    } else {
        s_step = (test_phase1_step_t)(s_step - 1U);
    }

    test_phase1_apply_step(s_step);
}

void Test_Phase1_Init(void)
{
    BSP_UART_Init();
    LED_Init();
    Key_Init(&s_key, KEY_DEFAULT_LONG_PRESS_TIME);
    bsp_ctrl_sd_init();
    bsp_pwm_init();

    BSP_UART_Printf("\r\n[P1] Phase1 start, Break temporarily disabled for PWM check\r\n");
    BSP_UART_Printf("\r\n[P1] KEY0 next step, KEY1 previous step, KEY2 all off\r\n");
    test_phase1_apply_step(s_step);
}

void Test_Phase1_Loop(void)
{
    uint32_t now = HAL_GetTick();

    if ((now - s_led_tick) >= TEST_PHASE1_LED_INTERVAL_MS) {
        s_led_tick = now;

        if (s_step == TEST_PHASE1_STEP_LOWER_ON) {
            LED0_TOGGLE();
        }

        if (s_step == TEST_PHASE1_STEP_ALL_OFF) {
            LED1_TOGGLE();
        }
    }

    if ((now - s_key_tick) >= TEST_PHASE1_KEY_INTERVAL_MS) {
        s_key_tick = now;
        Key_Scan(&s_key);

        if (Key_GetEvent(&s_key, 0U) == KEY_EVENT_SHORT_PRESS) {
            test_phase1_step_next();
        }

        if (Key_GetEvent(&s_key, 1U) == KEY_EVENT_SHORT_PRESS) {
            test_phase1_step_prev();
        }

        if (Key_GetEvent(&s_key, 2U) == KEY_EVENT_SHORT_PRESS) {
            s_step = TEST_PHASE1_STEP_ALL_OFF;
            test_phase1_apply_step(s_step);
        }
    }

    if (bsp_pwm_is_break_fault() != 0U) {
        BSP_UART_Printf("[P1] Break fault detected\r\n");
        bsp_pwm_clear_break_fault();
    }
}
