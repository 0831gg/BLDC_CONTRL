#include "test.h"

#include "bsp_adc_motor.h"
#include "bsp_current_sense.h"
#include "bsp_ctrl_sd.h"
#include "bsp_delay.h"
#include "bsp_hall.h"
#include "bsp_key.h"
#include "bsp_led.h"
#include "bsp_pwm.h"
#include "bsp_uart.h"
#include "bsp_cmd.h"
#include "motor_ctrl.h"
#include "tim.h"
#include "vofa_uart.h"

#define LED_INTERVAL_MS   200U
#define KEY_INTERVAL_MS   20U
#define VOFA_INTERVAL_MS  50U

static const uint8_t DUTY_PERCENT_TABLE[] = {
    0, 5, 10, 15, 20, 25, 30, 35, 40, 45,
    50, 55, 60, 65, 70, 75, 80, 85, 90, 95
};

#define DUTY_TABLE_SIZE  (sizeof(DUTY_PERCENT_TABLE) / sizeof(DUTY_PERCENT_TABLE[0]))

static const int32_t SPEED_TARGET_TABLE[] = {
    0, 200, 400, 600, 800, 1000, 1500, 2000, 2500, 3000
};
#define SPEED_TABLE_SIZE  (sizeof(SPEED_TARGET_TABLE) / sizeof(SPEED_TARGET_TABLE[0]))

static KeyGroup_HandleTypeDef s_key;
static uint32_t s_led_tick = 0U;
static uint32_t s_key_tick = 0U;
static uint32_t s_vofa_tick = 0U;
static uint8_t s_duty_index = 1U;  /* 初始占空比5%，缓启动 */
static uint8_t s_speed_index = 0U;
static uint8_t s_init_ok = 0U;

static inline uint16_t get_current_duty(void)
{
    return (BSP_PWM_PERIOD + 1U) * DUTY_PERCENT_TABLE[s_duty_index] / 100U;
}

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

static void send_vofa_waveform(void)
{
    bsp_current_t current;
    float channels[11];

    bsp_current_sense_get_filtered(&current);

    channels[0]  = (float)motor_ctrl_is_running();
    channels[1]  = (float)motor_ctrl_get_mode();
    channels[2]  = (float)bsp_hall_get_speed_signed();
    channels[3]  = (float)motor_ctrl_get_speed_target();
    channels[4]  = motor_ctrl_get_pid_output();
    channels[5]  = (float)motor_ctrl_get_duty() * 100.0f / (float)(BSP_PWM_PERIOD + 1U);  /* 占空比 % */
    channels[6]  = bsp_adc_get_vbus();
    channels[7]  = bsp_adc_get_temp();
    channels[8]  = current.u_ma;
    channels[9]  = current.v_ma;
    channels[10] = current.w_ma;

    VOFA_FireWaterFloat(channels, (uint8_t)(sizeof(channels) / sizeof(channels[0])));
}

static void start_motor(void)
{
    uint16_t duty;
    uint8_t dir;

    if (motor_ctrl_get_mode() == MOTOR_MODE_SPEED_PID) {
        /* PID模式：最小占空比启动，PID自动升速 */
        duty = (BSP_PWM_PERIOD + 1U) * 5U / 100U;
        dir = (motor_ctrl_get_speed_target() >= 0) ? MOTOR_DIR_CW : MOTOR_DIR_CCW;
    } else {
        duty = get_current_duty();
        dir = motor_ctrl_get_direction();
    }
    (void)motor_start_simple(duty, dir);
}

static void stop_motor(void)
{
    motor_stop();
}

static void toggle_direction(void)
{
    if (motor_ctrl_get_mode() == MOTOR_MODE_SPEED_PID) {
        /* PID模式：取反目标转速 */
        motor_ctrl_set_speed_target(-motor_ctrl_get_speed_target());
        return;
    }

    uint8_t new_dir = (motor_ctrl_get_direction() == MOTOR_DIR_CCW) ? MOTOR_DIR_CW : MOTOR_DIR_CCW;
    uint8_t was_running = motor_ctrl_is_running();

    if (was_running) {
        stop_motor();
    }

    motor_ctrl_set_direction(new_dir);

    if (was_running) {
        start_motor();
    }
}

static void adjust_duty(void)
{
    s_duty_index = (s_duty_index + 1U) % DUTY_TABLE_SIZE;
    motor_ctrl_set_duty(get_current_duty());
}



static void handle_keys(void)
{
    KeyEvent_TypeDef key0 = Key_GetEvent(&s_key, 0U);
    KeyEvent_TypeDef key1 = Key_GetEvent(&s_key, 1U);
    KeyEvent_TypeDef key2 = Key_GetEvent(&s_key, 2U);

    if (key0 == KEY_EVENT_SHORT_PRESS) {
        if (motor_ctrl_is_running()) {
            stop_motor();
        } else {
            start_motor();
        }
    }

    if (key1 == KEY_EVENT_SHORT_PRESS) {
        toggle_direction();
    }

    if (key2 == KEY_EVENT_SHORT_PRESS) {
        if (motor_ctrl_get_mode() == MOTOR_MODE_SPEED_PID) {
            /* PID模式：循环目标转速 */
            s_speed_index = (s_speed_index + 1U) % SPEED_TABLE_SIZE;
            int32_t target = SPEED_TARGET_TABLE[s_speed_index];
            /* 保持当前方向符号 */
            if (motor_ctrl_get_speed_target() < 0) {
                target = -target;
            }
            motor_ctrl_set_speed_target(target);
        } else {
            adjust_duty();
        }
    }

    if (key2 == KEY_EVENT_LONG_RELEASE) {
        /* 长按切换开环/PID模式 */
        uint8_t was_running = motor_ctrl_is_running();
        if (was_running) {
            stop_motor();
        }

        if (motor_ctrl_get_mode() == MOTOR_MODE_OPEN_LOOP) {
            motor_ctrl_set_mode(MOTOR_MODE_SPEED_PID);
            s_speed_index = 1U;  /* 从200 RPM开始，而不是0 */
            motor_ctrl_set_speed_target(SPEED_TARGET_TABLE[s_speed_index]);
        } else {
            motor_ctrl_set_mode(MOTOR_MODE_OPEN_LOOP);
        }
    }
}

static void handle_faults(void)
{
    if (bsp_pwm_is_break_fault()) {
        motor_ctrl_set_fault(MOTOR_FAULT_BREAK);
        stop_motor();
        bsp_pwm_clear_break_fault();
    }

    if (motor_ctrl_is_running() && (motor_ctrl_get_fault() == MOTOR_FAULT_HALL_INVALID)) {
        stop_motor();
    }
}

void Test_Init(void)
{
    Delay_Init();
    BSP_UART_Init();
    VOFA_Init();
    LED_Init();
    Key_Init(&s_key, KEY_DEFAULT_LONG_PRESS_TIME);

    bsp_ctrl_sd_init();
    bsp_pwm_init();

    if (bsp_hall_init() != 0) {
        LED0_ON();
        LED1_ON();
        return;
    }

    if (bsp_adc_motor_init() != 0) {
        LED0_ON();
        LED1_ON();
        return;
    }

    __HAL_TIM_SET_COMPARE(&htim1, TIM_CHANNEL_4, BSP_PWM_PERIOD / 2U);
    if (HAL_TIM_PWM_Start(&htim1, TIM_CHANNEL_4) != HAL_OK) {
        LED0_ON();
        LED1_ON();
        return;
    }

    if (bsp_current_sense_init() != 0) {
        LED0_ON();
        LED1_ON();
        return;
    }

    motor_ctrl_init();
    bsp_pwm_disable_break();
    bsp_cmd_init();

    s_init_ok = 1U;
}

void Test_Loop(void)
{
    uint32_t now = HAL_GetTick();

    if (!s_init_ok) {
        return;
    }

    if ((now - s_led_tick) >= LED_INTERVAL_MS) {
        s_led_tick = now;
        update_led();
    }

    if ((now - s_key_tick) >= KEY_INTERVAL_MS) {
        s_key_tick = now;
        Key_Scan(&s_key);
        handle_keys();
    }

    bsp_current_sense_update_filter();
    handle_faults();
    bsp_cmd_process();

    if (bsp_current_sense_is_overcurrent() && motor_ctrl_is_running()) {
        stop_motor();
    }

    if ((now - s_vofa_tick) >= VOFA_INTERVAL_MS) {
        s_vofa_tick = now;
        send_vofa_waveform();
    }
}
