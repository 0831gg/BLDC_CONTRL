#ifndef __BSP_LED_H
#define __BSP_LED_H

#include "main.h"

typedef enum {
    LED0,
    LED1,
    LED_MAX
} led_enum_e;

typedef enum {
    LED_OFF = 0,
    LED_ON = 1
} led_status_e;

typedef struct {
    led_enum_e num;
    GPIO_TypeDef* port;
    uint16_t pin;
} led_config_t;

void LED_Init(void);                                // LED初始化函数
void LED_Ctrl(led_enum_e num, led_status_e status); // LED控制函数，num为LED编号，status为LED状态（开或关）
void LED_Toggle(led_enum_e num);                    // LED切换函数，num为LED编号

#define LED0_ON()      LED_Ctrl(LED0, LED_ON)           // LED0开
#define LED0_OFF()     LED_Ctrl(LED0, LED_OFF)          // LED0关
#define LED0_TOGGLE()  LED_Toggle(LED0)                 // LED0切换

#define LED1_ON()      LED_Ctrl(LED1, LED_ON)       // LED1开
#define LED1_OFF()     LED_Ctrl(LED1, LED_OFF)      // LED1关
#define LED1_TOGGLE()  LED_Toggle(LED1)             // LED1切换

#endif
