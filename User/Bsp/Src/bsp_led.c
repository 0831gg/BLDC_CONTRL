/**
 * @file    bsp_led.c
 * @brief   LED驱动
 * 
 * @details 【用户配置】修改 led_config 数组即可配置LED的GPIO
 * 
 *          【使用方法】
 *          1. 初始化: LED_Init();
 *          2. 控制LED:
 *             LED0_ON();      // LED0亮
 *             LED0_OFF();     // LED0灭
 *             LED0_TOGGLE();  // LED0翻转
 *             LED1_ON();      // LED1亮
 *             LED1_OFF();     // LED1灭
 *             LED1_TOGGLE();  // LED1翻转
 *          3. 通用控制: LED_Ctrl(LED0, LED_ON);
 * 
 *          【添加新LED】
 *          1. 在 led_enum_e 枚举中添加 LED2
 *          2. 在 led_config 数组中添加配置
 *          3. 在头文件中添加宏定义
 */

#include "bsp_led.h"

/**
 * @brief LED GPIO配置 - 用户只需修改这里
 * @note  格式: {LED编号, GPIO端口, 引脚号}
 */
static const led_config_t led_config[] = {
    {LED0, LED0_GPIO_Port, LED0_Pin},    // LED0: 红灯
    {LED1, LED1_GPIO_Port, LED1_Pin},    // LED1: 绿灯
    {LED_MAX, NULL, 0}  // 结束标志
};

/**
 * @brief 初始化所有LED，默认关闭
 */
void LED_Init(void) {
    for (uint8_t i = 0; i < LED_MAX; i++) {
        if (led_config[i].port != NULL) {
            HAL_GPIO_WritePin(led_config[i].port, led_config[i].pin, GPIO_PIN_SET);
        }
    }
}

/**
 * @brief  控制LED开关
 * @param  num    LED编号 (LED0, LED1)
 * @param  status 状态 (LED_ON, LED_OFF)
 */
void LED_Ctrl(led_enum_e num, led_status_e status) {
    for (uint8_t i = 0; i < LED_MAX; i++) {
        if (led_config[i].num == num && led_config[i].port != NULL) {
            if (status == LED_ON) {
                HAL_GPIO_WritePin(led_config[i].port, led_config[i].pin, GPIO_PIN_RESET);
            } else {
                HAL_GPIO_WritePin(led_config[i].port, led_config[i].pin, GPIO_PIN_SET);
            }
        }
    }
}

/**
 * @brief  LED翻转
 * @param  num LED编号
 */
void LED_Toggle(led_enum_e num) {
    for (uint8_t i = 0; i < LED_MAX; i++) {
        if (led_config[i].num == num && led_config[i].port != NULL) {
            HAL_GPIO_TogglePin(led_config[i].port, led_config[i].pin);
        }
    }
}
