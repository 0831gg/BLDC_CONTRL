/**
 * @file    test_phase0.h
 * @brief   Phase 0 基础硬件自检测试
 * 
 * 测试内容：
 *   - USART1 TX + DMA 链路 (BSP_UART_Printf)
 *   - Vofa 上位机数据接收 (JustFloat / FireWater)
 *   - LED 闪烁
 *   - 按键扫描
 *   - DWT 延时精度
 */

#ifndef __TEST_PHASE0_H
#define __TEST_PHASE0_H

#ifdef __cplusplus
extern "C" {
#endif

#include "main.h"

/**
 * @brief  Phase 0 初始化 (在 main 的 USER CODE BEGIN 2 中调用一次)
 */
void Test_Phase0_Init(void);

/**
 * @brief  Phase 0 主循环 (在 main 的 while(1) 中调用)
 */
void Test_Phase0_Loop(void);

#ifdef __cplusplus
}
#endif

#endif /* __TEST_PHASE0_H */
