#ifndef __BSP_DELAY_H   
#define __BSP_DELAY_H

#include "main.h"










extern uint32_t Delay_Init(void);// DWT 计数器初始化函数
extern void Delay_ms(uint32_t ms);// 毫秒级延时函数
extern void Delay_us(uint32_t us);// 微秒级延时函数
extern void Delay_us_test(uint8_t us);// 微秒级延时测试函数




#endif // __DELAY_H
