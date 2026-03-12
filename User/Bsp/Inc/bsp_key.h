#ifndef __BSP_KEY_H
#define __BSP_KEY_H

#include "main.h"

#define KEY_NUM  3

/** @brief Key state enumeration */
typedef enum {
    KEY_IDLE = 0,
    KEY_PRESSED,
    KEY_SHORT_RELEASE,
    KEY_LONG_PRESS,
    KEY_LONG_RELEASE
} KeyState_TypeDef;

/** @brief Key event enumeration */
typedef enum {
    KEY_EVENT_NONE = 0,
    KEY_EVENT_SHORT_PRESS,
    KEY_EVENT_LONG_PRESS,
    KEY_EVENT_LONG_RELEASE
} KeyEvent_TypeDef;


/** @brief Key handle structure */
typedef struct {
    GPIO_TypeDef *port;
    uint16_t pin;
    KeyState_TypeDef state;
    uint16_t counter;
    uint8_t pressed;
} Key_HandleTypeDef;


/** @brief Key group handle structure */
typedef struct {
    Key_HandleTypeDef keys[KEY_NUM];
    uint16_t long_press_time;
} KeyGroup_HandleTypeDef;



#define KEY_TIME_20MS     1               // 20ms 对应的计数值 (假设扫描周期为 20ms)
#define KEY_TIME_100MS    5               // 100ms 对应的计数值 (5 * 20ms = 100ms)
#define KEY_TIME_500MS   25               // 500ms 对应的计数值 (25 * 20ms = 500ms)
#define KEY_TIME_1000MS  50               // 1000ms 对应的计数值 (50 * 20ms = 1000ms)
#define KEY_TIME_2000MS 100               // 2000ms 对应的计数值 (100 * 20ms = 2000ms)

#define KEY_DEFAULT_LONG_PRESS_TIME  KEY_TIME_1000MS                             // 默认长按时间为 1000ms (1秒)

void Key_Init(KeyGroup_HandleTypeDef *hkey, uint16_t long_press_time);          // 初始化按键
void Key_Scan(KeyGroup_HandleTypeDef *hkey);                                    // 扫描按键状态，需在定时器中定期调用
KeyEvent_TypeDef Key_GetEvent(KeyGroup_HandleTypeDef *hkey, uint8_t key_id);    // 获取按键事件
uint8_t Key_IsPressed(KeyGroup_HandleTypeDef *hkey, uint8_t key_id);            // 检查按键是否处于按下状态

#define KEY_READ(port, pin)  HAL_GPIO_ReadPin(port, pin)                        // 读取按键状态

#endif
