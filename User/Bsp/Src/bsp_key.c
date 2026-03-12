#include "bsp_key.h"

/** @brief 按键事件数组 */
static KeyEvent_TypeDef key_event[KEY_NUM] = {KEY_EVENT_NONE};



/** @brief      初始化按键
 * @param hkey  按键组句柄
 * @param long_press_time 长按时间
 */
void Key_Init(KeyGroup_HandleTypeDef *hkey, uint16_t long_press_time)
{
    uint8_t i;

    hkey->keys[0].port = KEY0_GPIO_Port;
    hkey->keys[0].pin  = KEY0_Pin;
    hkey->keys[1].port = KEY1_GPIO_Port;
    hkey->keys[1].pin  = KEY1_Pin;
    hkey->keys[2].port = KEY2_GPIO_Port;
    hkey->keys[2].pin  = KEY2_Pin;

    for (i = 0; i < KEY_NUM; i++) {
        hkey->keys[i].state = KEY_IDLE;
        hkey->keys[i].counter = 0;
        hkey->keys[i].pressed = 0;
        key_event[i] = KEY_EVENT_NONE;
    }

    hkey->long_press_time = (long_press_time > 0) ? long_press_time : KEY_DEFAULT_LONG_PRESS_TIME;
}



/** @brief      扫描按键状态
 * @param hkey  按键组句柄
 */
void Key_Scan(KeyGroup_HandleTypeDef *hkey)
{
    uint8_t i;
    uint8_t pin_state;

    for (i = 0; i < KEY_NUM; i++) {
        pin_state = KEY_READ(hkey->keys[i].port, hkey->keys[i].pin);

        switch (hkey->keys[i].state) {
            case KEY_IDLE:
                if (pin_state == 0) {
                    hkey->keys[i].state = KEY_PRESSED;
                    hkey->keys[i].counter = 0;
                }
                break;

            case KEY_PRESSED:
                hkey->keys[i].counter++;
                if (hkey->keys[i].counter >= KEY_TIME_20MS) {
                    if (pin_state == 0) {
                        hkey->keys[i].pressed = 1;
                        hkey->keys[i].state = KEY_SHORT_RELEASE;
                    } else {
                        hkey->keys[i].state = KEY_IDLE;
                        hkey->keys[i].counter = 0;
                    }
                }
                break;

            case KEY_SHORT_RELEASE:
                hkey->keys[i].counter++;
                if (pin_state == 1) {
                    key_event[i] = KEY_EVENT_SHORT_PRESS;
                    hkey->keys[i].state = KEY_IDLE;
                    hkey->keys[i].counter = 0;
                    hkey->keys[i].pressed = 0;
                } else if (hkey->keys[i].counter >= hkey->long_press_time) {
                    key_event[i] = KEY_EVENT_LONG_PRESS;
                    hkey->keys[i].state = KEY_LONG_PRESS;
                }
                break;

            case KEY_LONG_PRESS:
                if (pin_state == 1) {
                    key_event[i] = KEY_EVENT_LONG_RELEASE;
                    hkey->keys[i].state = KEY_LONG_RELEASE;
                }
                break;

            case KEY_LONG_RELEASE:
                hkey->keys[i].counter++;
                if (hkey->keys[i].counter >= KEY_TIME_20MS) {
                    hkey->keys[i].state = KEY_IDLE;
                    hkey->keys[i].counter = 0;
                    hkey->keys[i].pressed = 0;
                }
                break;

            default:
                hkey->keys[i].state = KEY_IDLE;
                break;
        }
    }
}



/** @brief         获取按键事件
 * @param hkey     按键组句柄
 * @param key_id   按键ID
 * @return         按键事件
 */ 
KeyEvent_TypeDef Key_GetEvent(KeyGroup_HandleTypeDef *hkey, uint8_t key_id)
{
    KeyEvent_TypeDef event;

    if (key_id >= KEY_NUM) {
        return KEY_EVENT_NONE;
    }

    event = key_event[key_id];
    key_event[key_id] = KEY_EVENT_NONE;

    return event;
}



/** @brief       检查按键是否处于按下状态
 * @param hkey   按键组句柄
 * @param key_id 按键ID
 * @return       1表示按键被按下，0表示未按下
 */
uint8_t Key_IsPressed(KeyGroup_HandleTypeDef *hkey, uint8_t key_id)
{
    if (key_id >= KEY_NUM) {
        return 0;
    }

    return hkey->keys[key_id].pressed;
}
