/**
 * @file    bsp_uart.h
 * @brief   UART调试通信
 */

#ifndef __BSP_UART_H
#define __BSP_UART_H

#ifdef __cplusplus
extern "C" {
#endif

#include "main.h"

void BSP_UART_Init(void);                           // UART初始化函数
void BSP_UART_SendByte(uint8_t data);               // 发送单字节函数，data为要发送的数据
void BSP_UART_SendString(const char *str);          // 发送字符串函数
void BSP_UART_SendData(uint8_t *data, uint16_t len);// 发送数据函数，data为数据指针，len为数据长度
uint8_t BSP_UART_ReceiveByte(void);                 // 接收单字节函数，返回接收到的数据
void BSP_UART_Printf(const char *fmt, ...);         // UART printf函数，支持格式化输出，fmt为格式字符串，...为可变参数列表

#ifdef __cplusplus
}
#endif

#endif /* __BSP_UART_H */
