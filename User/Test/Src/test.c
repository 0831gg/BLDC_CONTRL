#include "test.h"

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

/*=============================================================================
 *  配置参数
 *=============================================================================*/

#define LED_INTERVAL_MS       200U
#define KEY_INTERVAL_MS       20U
#define PRINT_INTERVAL_MS     800U

/* 占空比表：0%, 8%, 10%, 15%, 20%...95% (步进5%) */
static const uint8_t DUTY_PERCENT_TABLE[] = {
    0,5, 10, 15, 20, 25, 30, 35, 40, 45, 50, 55, 60, 65, 70, 75, 80,85,90,95
};
#define DUTY_TABLE_SIZE  (sizeof(DUTY_PERCENT_TABLE) / sizeof(DUTY_PERCENT_TABLE[0]))

/*=============================================================================
 *  私有变量
 *=============================================================================*/

static KeyGroup_HandleTypeDef s_key;
static uint32_t s_led_tick = 0U;
static uint32_t s_key_tick = 0U;
static uint32_t s_print_tick = 0U;
static uint8_t s_duty_index = 0U;
static uint8_t s_init_ok = 0U;

/*=============================================================================
 *  辅助函数
 *=============================================================================*/

/**
 * @brief  获取当前占空比（PWM计数值）
 */
static inline uint16_t get_current_duty(void)
{
    return (BSP_PWM_PERIOD + 1U) * DUTY_PERCENT_TABLE[s_duty_index] / 100U;
}

/**
 * @brief  格式化霍尔状态为字符串
 * @param  state: 霍尔状态（0-7）
 * @param  buf: 输出缓冲区（至少4字节）
 */
static void format_hall_state(uint8_t state, char *buf)
{
    buf[0] = '0' + ((state >> 2) & 0x01U);
    buf[1] = '0' + ((state >> 1) & 0x01U);
    buf[2] = '0' + (state & 0x01U);
    buf[3] = '\0';
}

/**
 * @brief  LED指示：运行时LED0闪烁，停止时LED1闪烁
 */
static void update_led(void)
{
    if (motor_ctrl_is_running()) {
        LED1_OFF();
        LED0_TOGGLE();
    } else {
        LED0_OFF();
        LED1_TOGGLE();
    }
}

/**
 * @brief  打印运行状态
 */
static void print_status(void)
{
    uint8_t hall_state = bsp_hall_get_state();
    char hall_str[4];
    uint32_t vbus_mv = (uint32_t)(bsp_adc_get_vbus() * 1000.0f);

    format_hall_state(hall_state, hall_str);

    BSP_UART_Printf("[P5] Run=%u Dir=%s Hall=%s Duty=%u%% RPM=%lu VBUS=%lu.%03luV\r\n",
                    motor_ctrl_is_running(),
                    (motor_ctrl_get_direction() == MOTOR_DIR_CCW) ? "CCW" : "CW",
                    hall_str,
                    DUTY_PERCENT_TABLE[s_duty_index],
                    bsp_hall_get_speed(),
                    vbus_mv / 1000U,
                    vbus_mv % 1000U);
}

/**
 * @brief  启动电机
 */
static void start_motor(void)
{
    uint16_t duty = get_current_duty();

    if (motor_start_simple(duty, motor_ctrl_get_direction()) == 0) {
        BSP_UART_Printf("[P5] Motor started, duty=%u%%\r\n", DUTY_PERCENT_TABLE[s_duty_index]);
    } else {
        BSP_UART_Printf("[P5] Motor start failed\r\n");
    }
}

/**
 * @brief  停止电机
 */
static void stop_motor(void)
{
    motor_stop();
    BSP_UART_Printf("[P5] Motor stopped\r\n");
}

/**
 * @brief  切换方向
 */
static void toggle_direction(void)
{
    uint8_t new_dir = (motor_ctrl_get_direction() == MOTOR_DIR_CCW) ? MOTOR_DIR_CW : MOTOR_DIR_CCW;
    uint8_t was_running = motor_ctrl_is_running();

    if (was_running) {
        stop_motor();
    }

    motor_ctrl_set_direction(new_dir);
    BSP_UART_Printf("[P5] Direction=%s\r\n", (new_dir == MOTOR_DIR_CCW) ? "CCW" : "CW");

    if (was_running) {
        start_motor();
    }
}

/**
 * @brief  调整占空比
 */
static void adjust_duty(void)
{
    s_duty_index = (s_duty_index + 1U) % DUTY_TABLE_SIZE;
    motor_ctrl_set_duty(get_current_duty());
    BSP_UART_Printf("[P5] Duty=%u%%\r\n", DUTY_PERCENT_TABLE[s_duty_index]);
}

/**
 * @brief  调整换相偏移
 */
static void adjust_offset(void)
{
    uint8_t new_offset = (motor_phase_tab_get_commutation_offset() + 1U) % 6U;
    uint8_t was_running = motor_ctrl_is_running();

    if (was_running) {
        stop_motor();
    }

    motor_phase_tab_set_commutation_offset(new_offset);
    BSP_UART_Printf("[P5] Offset=%u\r\n", new_offset);

    if (was_running) {
        start_motor();
    }
}

/**
 * @brief  处理按键事件
 */
static void handle_keys(void)
{
    KeyEvent_TypeDef key0 = Key_GetEvent(&s_key, 0U);
    KeyEvent_TypeDef key1 = Key_GetEvent(&s_key, 1U);
    KeyEvent_TypeDef key2 = Key_GetEvent(&s_key, 2U);

    /* KEY0: 启动/停止 */
    if (key0 == KEY_EVENT_SHORT_PRESS) {
        if (motor_ctrl_is_running()) {
            stop_motor();
        } else {
            start_motor();
        }
    }

    /* KEY1: 切换方向 */
    if (key1 == KEY_EVENT_SHORT_PRESS) {
        toggle_direction();
    }

    /* KEY2短按: 调整占空比 */
    if (key2 == KEY_EVENT_SHORT_PRESS) {
        adjust_duty();
    }

    /* KEY2长按: 调整换相偏移 */
    if (key2 == KEY_EVENT_LONG_PRESS) {
        adjust_offset();
    }
}

/**
 * @brief  处理故障
 */
static void handle_faults(void)
{
    /* 硬件故障（过流等） */
    if (bsp_pwm_is_break_fault()) {
        motor_ctrl_set_fault(MOTOR_FAULT_BREAK);
        stop_motor();
        BSP_UART_Printf("[P5] Break fault!\r\n");
        bsp_pwm_clear_break_fault();
    }

    /* 霍尔无效故障 */
    if (motor_ctrl_is_running() && (motor_ctrl_get_fault() == MOTOR_FAULT_HALL_INVALID)) {
        stop_motor();
        BSP_UART_Printf("[P5] Hall invalid!\r\n");
    }
}

/*=============================================================================
 *  公共函数
 *=============================================================================*/

/**
 * @brief  初始化测试模块
 */
void Test_Init(void)
{
    /* 基础初始化 */
    Delay_Init();
    BSP_UART_Init();
    LED_Init();
    Key_Init(&s_key, KEY_DEFAULT_LONG_PRESS_TIME);

    BSP_UART_Printf("\r\n[P5] BLDC Motor Control Test\r\n");

    /* 硬件初始化 */
    bsp_ctrl_sd_init();
    bsp_pwm_init();

    if (bsp_hall_init() != 0) {
        BSP_UART_Printf("[P5] ERROR: Hall init failed\r\n");
        LED0_ON();
        LED1_ON();
        return;
    }

    if (bsp_adc_motor_init() != 0) {
        BSP_UART_Printf("[P5] ERROR: ADC init failed\r\n");
        LED0_ON();
        LED1_ON();
        return;
    }

    /* 启动ADC触发 */
    __HAL_TIM_SET_COMPARE(&htim1, TIM_CHANNEL_4, BSP_PWM_PERIOD / 2U);
    if (HAL_TIM_PWM_Start(&htim1, TIM_CHANNEL_4) != HAL_OK) {
        BSP_UART_Printf("[P5] ERROR: ADC trigger failed\r\n");
        LED0_ON();
        LED1_ON();
        return;
    }

    /* 电机控制初始化 */
    motor_ctrl_init();
    bsp_pwm_disable_break();  /* 测试时禁用硬件保护 */

    /* 打印配置信息 */
    BSP_UART_Printf("[P5] KEY0=Start/Stop, KEY1=Direction, KEY2=Duty/Offset\r\n");
    BSP_UART_Printf("[P5] Duty range: 8%%-80%%, step: 5%%\r\n");
    BSP_UART_Printf("[P5] Ready!\r\n\r\n");

    s_init_ok = 1U;
    print_status();
}

/**
 * @brief  主循环
 */
void Test_Loop(void)
{
    uint32_t now = HAL_GetTick();

    if (!s_init_ok) {
        return;
    }

    /* LED更新 (200ms) */
    if ((now - s_led_tick) >= LED_INTERVAL_MS) {
        s_led_tick = now;
        update_led();
    }

    /* 按键扫描 (20ms) */
    if ((now - s_key_tick) >= KEY_INTERVAL_MS) {
        s_key_tick = now;
        Key_Scan(&s_key);
        handle_keys();
    }

    /* 故障处理 */
    handle_faults();

    /* 状态打印 (800ms) */
    if ((now - s_print_tick) >= PRINT_INTERVAL_MS) {
        s_print_tick = now;
        print_status();
    }
}
