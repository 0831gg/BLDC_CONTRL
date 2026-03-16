/**
 * @file    test.h
 * @brief   BLDC电机测试程序
 * @details 电机控制测试程序，整合了所有功能:
 *          - 霍尔传感器读取
 *          - PWM调速控制
 *          - ADC采样（VBUS、温度、相电流）
 *          - 按键控制
 *          - 状态显示
 *
 * @note    按键功能:
 *          - KEY0: 启动/停止电机
 *          - KEY1: 切换旋转方向
 *          - KEY2: 短按切换占空比等级，长按调整换相偏移
 *
 * @note    LED指示:
 *          - LED0亮: 电机运行中（闪烁）
 *          - LED1亮: 电机停止（闪烁）
 *          - LED0/LED1同时亮: 初始化失败
 */

#ifndef __TEST_H
#define __TEST_H

#ifdef __cplusplus
extern "C" {
#endif

#include "main.h"

void Test_Init(void);
void Test_Loop(void);

#ifdef __cplusplus
}
#endif

#endif /* __TEST_H */
