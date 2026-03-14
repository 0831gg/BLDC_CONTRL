#include "test_phase4.h"

#include "bsp_ctrl_sd.h"
#include "bsp_delay.h"
#include "bsp_hall.h"
#include "bsp_key.h"
#include "bsp_led.h"
#include "bsp_pwm.h"
#include "bsp_uart.h"
#include "motor_ctrl.h"

#define TEST_PHASE4_LED_INTERVAL_MS       (200U)
#define TEST_PHASE4_KEY_INTERVAL_MS       (20U)
#define TEST_PHASE4_PRINT_INTERVAL_MS     (300U)

#define TEST_PHASE4_DUTY_1                ((BSP_PWM_PERIOD + 1U) * 5U / 100U)
#define TEST_PHASE4_DUTY_2                ((BSP_PWM_PERIOD + 1U) * 75U / 1000U)
#define TEST_PHASE4_DUTY_3                ((BSP_PWM_PERIOD + 1U) * 10U / 100U)

static const uint16_t s_duty_table[] = {
    TEST_PHASE4_DUTY_1,
    TEST_PHASE4_DUTY_2,
    TEST_PHASE4_DUTY_3
};

static KeyGroup_HandleTypeDef s_key;
static uint32_t s_led_tick = 0U;
static uint32_t s_key_tick = 0U;
static uint32_t s_print_tick = 0U;
static uint8_t s_hall_init_status = 0U;
static uint8_t s_duty_index = 0U;

static const char *test_phase4_dir_text(uint8_t direction)
{
    return (direction == MOTOR_DIR_CCW) ? "CCW" : "CW";
}

static void test_phase4_refresh_led(void)
{
    if (motor_ctrl_is_running() != 0U) {
        LED1_OFF();
        LED0_TOGGLE();
    } else {
        LED0_OFF();
        LED1_TOGGLE();
    }
}

static void test_phase4_start_motor(void)
{
    if (motor_start(s_duty_table[s_duty_index], motor_ctrl_get_direction()) == 0) {
        BSP_UART_Printf("[P4] Motor start dir=%s duty=%u action=%s\r\n",
                        test_phase4_dir_text(motor_ctrl_get_direction()),
                        (unsigned int)motor_ctrl_get_duty(),
                        motor_phase_action_name(motor_ctrl_get_last_action()));
    } else {
        BSP_UART_Printf("[P4] Motor start failed fault=%u hall=%u%u%u\r\n",
                        (unsigned int)motor_ctrl_get_fault(),
                        (unsigned int)((bsp_hall_get_state() >> 2) & 0x01U),
                        (unsigned int)((bsp_hall_get_state() >> 1) & 0x01U),
                        (unsigned int)(bsp_hall_get_state() & 0x01U));
    }
}

void Test_Phase4_Init(void)
{
    Delay_Init();
    BSP_UART_Init();
    BSP_UART_Printf("\r\n[P4] Boot\r\n");

    LED_Init();
    Key_Init(&s_key, KEY_DEFAULT_LONG_PRESS_TIME);
    bsp_ctrl_sd_init();
    bsp_pwm_init();

    s_hall_init_status = bsp_hall_init();
    if (s_hall_init_status != 0U) {
        LED0_ON();
        LED1_ON();
        BSP_UART_Printf("[P4] Hall init failed, code=%u\r\n", (unsigned int)s_hall_init_status);
        return;
    }

    motor_ctrl_init();

    BSP_UART_Printf("\r\n[P4] Phase4 start\r\n");
    BSP_UART_Printf("[P4] Low-risk only: limited duty, low bus, stop on abnormal current/noise\r\n");
    BSP_UART_Printf("[P4] KEY0 start/stop, KEY1 dir toggle, KEY2 duty step\r\n");
    BSP_UART_Printf("[P4] Hall display order=UVW, inject PA0=U PA1=V PA2=W\r\n");
    BSP_UART_Printf("[P4] CCW seq: 001->101->100->110->010->011->001\r\n");
    BSP_UART_Printf("[P4] CW  seq: 001->011->010->110->100->101->001\r\n");
    BSP_UART_Printf("[P4] Init Hall=%u%u%u Dir=%s Duty=%u\r\n",
                    (unsigned int)((bsp_hall_get_state() >> 2) & 0x01U),
                    (unsigned int)((bsp_hall_get_state() >> 1) & 0x01U),
                    (unsigned int)(bsp_hall_get_state() & 0x01U),
                    test_phase4_dir_text(motor_ctrl_get_direction()),
                    (unsigned int)s_duty_table[s_duty_index]);
}

void Test_Phase4_Loop(void)
{
    uint32_t now = HAL_GetTick();
    uint8_t state;

    if (s_hall_init_status != 0U) {
        return;
    }

    if ((now - s_led_tick) >= TEST_PHASE4_LED_INTERVAL_MS) {
        s_led_tick = now;
        test_phase4_refresh_led();
    }

    if ((now - s_key_tick) >= TEST_PHASE4_KEY_INTERVAL_MS) {
        s_key_tick = now;
        Key_Scan(&s_key);

        if (Key_GetEvent(&s_key, 0U) == KEY_EVENT_SHORT_PRESS) {
            if (motor_ctrl_is_running() != 0U) {
                motor_stop();
                BSP_UART_Printf("[P4] Motor stop\r\n");
            } else {
                test_phase4_start_motor();
            }
        }

        if (Key_GetEvent(&s_key, 1U) == KEY_EVENT_SHORT_PRESS) {
            uint8_t next_dir = (motor_ctrl_get_direction() == MOTOR_DIR_CCW) ? MOTOR_DIR_CW : MOTOR_DIR_CCW;
            motor_ctrl_set_direction(next_dir);
            BSP_UART_Printf("[P4] Direction=%s\r\n", test_phase4_dir_text(next_dir));

            if (motor_ctrl_is_running() != 0U) {
                motor_stop();
                test_phase4_start_motor();
            }
        }

        if (Key_GetEvent(&s_key, 2U) == KEY_EVENT_SHORT_PRESS) {
            s_duty_index = (uint8_t)((s_duty_index + 1U) % (sizeof(s_duty_table) / sizeof(s_duty_table[0])));
            motor_ctrl_set_duty(s_duty_table[s_duty_index]);
            BSP_UART_Printf("[P4] Duty=%u\r\n", (unsigned int)s_duty_table[s_duty_index]);
        }
    }

    if (bsp_pwm_is_break_fault() != 0U) {
        motor_ctrl_set_fault(MOTOR_FAULT_BREAK);
        motor_stop();
        BSP_UART_Printf("[P4] Break fault detected\r\n");
        bsp_pwm_clear_break_fault();
    }

    if ((now - s_print_tick) >= TEST_PHASE4_PRINT_INTERVAL_MS) {
        s_print_tick = now;
        state = bsp_hall_get_state();

        BSP_UART_Printf("[P4] Run=%u Dir=%s Duty=%u Hall=%u%u%u Act=%s Fault=%u Edge=%lu Valid=%lu Invalid=%lu\r\n",
                        (unsigned int)motor_ctrl_is_running(),
                        test_phase4_dir_text(motor_ctrl_get_direction()),
                        (unsigned int)motor_ctrl_get_duty(),
                        (unsigned int)((state >> 2) & 0x01U),
                        (unsigned int)((state >> 1) & 0x01U),
                        (unsigned int)(state & 0x01U),
                        motor_phase_action_name(motor_ctrl_get_last_action()),
                        (unsigned int)motor_ctrl_get_fault(),
                        (unsigned long)bsp_hall_get_edge_count(),
                        (unsigned long)bsp_hall_get_valid_transition_count(),
                        (unsigned long)bsp_hall_get_invalid_transition_count());
    }
}
