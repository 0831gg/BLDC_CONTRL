#ifndef __VOFA_UART_H
#define __VOFA_UART_H

#include "vofa_port.h"
#include <stdint.h>
#include <stdarg.h>

/*=============================================================================
 *  VOFA+ 协议层 (精简版)
 *
 *  JustFloat : float[N] + {0x00,0x00,0x80,0x7F}  — 二进制波形, 高效
 *  FireWater : "ch0, ch1, ..., chN\n"             — 文本波形 + 调试混用
 *
 *  两种协议不可同时使用, Vofa+ 上位机需选择对应协议
 *  波特率由 CubeMX 配置 (115200), Vofa+ 上位机需设同样波特率
 *=============================================================================*/

/** 初始化 (标记就绪, 不修改波特率) */
void VOFA_Init(void);

/**
 * @brief  JustFloat 协议 — 发送多通道浮点波形
 * @param  data:     float 数组
 * @param  ch_count: 通道数
 */
void VOFA_JustFloat(float* data, uint8_t ch_count);

/**
 * @brief  FireWater 协议 — printf 格式打印 (文本 + 波形)
 */
void VOFA_Printf(const char* fmt, ...);

/**
 * @brief  FireWater 协议 — 发送多通道浮点波形
 * @param  data:     float 数组
 * @param  ch_count: 通道数
 * @note   输出格式: "v0, v1, ..., vN\n"
 */
void VOFA_FireWaterFloat(float* data, uint8_t ch_count);

/** 发送原始字节 */
void VOFA_SendRaw(const uint8_t* data, uint16_t len);

#endif /* __VOFA_UART_H */
