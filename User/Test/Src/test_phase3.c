#include "test_phase3.h"

#include "bsp_hall.h"
#include "bsp_key.h"
#include "bsp_led.h"
#include "bsp_uart.h"

#define TEST_PHASE3_LED_INTERVAL_MS     (200U)
#define TEST_PHASE3_KEY_INTERVAL_MS     (20U)
#define TEST_PHASE3_PRINT_INTERVAL_MS   (300U)
#define TEST_PHASE3_ERROR_TIM5_START    (1U)

static KeyGroup_HandleTypeDef s_key;
static uint32_t s_led_tick = 0U;
static uint32_t s_key_tick = 0U;
static uint32_t s_print_tick = 0U;
static uint8_t s_hall_init_status = 0U;

static const char *test_phase3_get_dir_text(int8_t dir)
{
    if (dir > 0) {
        return "CW";
    }

    if (dir < 0) {
        return "CCW";
    }

    return "UNK";
}

static void test_phase3_refresh_led(void)
{
    if (bsp_hall_is_irq_enabled() != 0U) {
        LED1_OFF();
        LED0_TOGGLE();
    } else {
        LED0_OFF();
        LED1_TOGGLE();
    }
}

static void test_phase3_print_error(uint8_t error_code)
{
    BSP_UART_Printf("[P3] Hall init failed, code=%u\r\n", (unsigned int)error_code);

    if (error_code == TEST_PHASE3_ERROR_TIM5_START) {
        BSP_UART_Printf("[P3] TIM5 start failed\r\n");
    }
}

void Test_Phase3_Init(void)
{
    BSP_UART_Init();
    BSP_UART_Printf("\r\n[P3] Boot\r\n");

    LED_Init();
    Key_Init(&s_key, KEY_DEFAULT_LONG_PRESS_TIME);

    s_hall_init_status = bsp_hall_init();
    if (s_hall_init_status != 0U) {
        LED0_ON();
        LED1_ON();
        test_phase3_print_error(s_hall_init_status);
        return;
    }

    BSP_UART_Printf("\r\n[P3] Phase3 start\r\n");
    BSP_UART_Printf("[P3] Hand-rotate motor, no power stage output\r\n");
    BSP_UART_Printf("[P3] KEY0 toggle Hall IRQ, KEY1 clear stats\r\n");
    BSP_UART_Printf("[P3] Hall init state=%u%u%u IRQ=%u\r\n",
                    (unsigned int)((bsp_hall_get_state() >> 2) & 0x01U),
                    (unsigned int)((bsp_hall_get_state() >> 1) & 0x01U),
                    (unsigned int)(bsp_hall_get_state() & 0x01U),
                    (unsigned int)bsp_hall_is_irq_enabled());
}

void Test_Phase3_Loop(void)
{
    uint32_t now = HAL_GetTick();

    if (s_hall_init_status != 0U) {
        return;
    }

    if ((now - s_led_tick) >= TEST_PHASE3_LED_INTERVAL_MS) {
        s_led_tick = now;
        test_phase3_refresh_led();
    }

    if ((now - s_key_tick) >= TEST_PHASE3_KEY_INTERVAL_MS) {
        s_key_tick = now;
        Key_Scan(&s_key);

        if (Key_GetEvent(&s_key, 0U) == KEY_EVENT_SHORT_PRESS) {
            if (bsp_hall_is_irq_enabled() != 0U) {
                bsp_hall_irq_disable();
                BSP_UART_Printf("[P3] Hall IRQ disabled\r\n");
            } else {
                bsp_hall_irq_enable();
                BSP_UART_Printf("[P3] Hall IRQ enabled\r\n");
            }
        }

        if (Key_GetEvent(&s_key, 1U) == KEY_EVENT_SHORT_PRESS) {
            bsp_hall_clear_stats();
            BSP_UART_Printf("[P3] Hall stats cleared\r\n");
        }
    }

    if ((now - s_print_tick) >= TEST_PHASE3_PRINT_INTERVAL_MS) {
        uint8_t state;
        uint8_t last_state;

        s_print_tick = now;
        state = bsp_hall_get_state();
        last_state = bsp_hall_get_last_state();

        BSP_UART_Printf("[P3] IRQ=%u Hall=%u%u%u Last=%u%u%u Dir=%s dt=(%luus) RPM=%lu Edge=%lu Valid=%lu Invalid=%lu T=%lu\r\n",
                        (unsigned int)bsp_hall_is_irq_enabled(),
                        (unsigned int)((state >> 2) & 0x01U),
                        (unsigned int)((state >> 1) & 0x01U),
                        (unsigned int)(state & 0x01U),
                        (unsigned int)((last_state >> 2) & 0x01U),
                        (unsigned int)((last_state >> 1) & 0x01U),
                        (unsigned int)(last_state & 0x01U),
                        test_phase3_get_dir_text(bsp_hall_get_direction()),
                        (unsigned long)bsp_hall_get_period_us(),
                        (unsigned long)bsp_hall_get_speed(),
                        (unsigned long)bsp_hall_get_edge_count(),
                        (unsigned long)bsp_hall_get_valid_transition_count(),
                        (unsigned long)bsp_hall_get_invalid_transition_count(),
                        (unsigned long)bsp_hall_get_commutation_time());
    }
}
