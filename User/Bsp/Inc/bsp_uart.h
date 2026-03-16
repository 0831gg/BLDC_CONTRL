/**
 * @file    bsp_uart.h
 * @brief   UART调试通信接口
 * @details 本模块提供UART1的初始化和通信功能，用于调试信息输出
 *          支持字节、字符串、数据块发送，以及格式化打印
 *          支持DMA和阻塞两种发送模式
 *
 * @note    UART配置 (USART1):
 *          - 波特率: 115200
 *          - 数据位: 8
 *          - 停止位: 1
 *          - 校验位: 无
 *
 * @note    使用方法:
 *          - 初始化: BSP_UART_Init();
 *          - 打印信息: BSP_UART_Printf("Value: %d\r\n", value);
 *          - 发送字符串: BSP_UART_SendString("Hello\r\n");
 */

#ifndef __BSP_UART_H
#define __BSP_UART_H

#ifdef __cplusplus
extern "C" {
#endif

#include "main.h"

/*=============================================================================
 *  DMA配置
 *=============================================================================*/

/**
 * @brief 是否使用DMA发送
 * @note  0 = 阻塞发送, 1 = DMA发送
 * @warning 使用DMA前必须在CubeMX中配置USART1 DMA TX
 */
#define BSP_UART_USE_DMA        1

/**
 * @brief DMA发送超时时间(ms)
 */
#define BSP_UART_DMA_TIMEOUT_MS 10

/*=============================================================================
 *  API函数
 *=============================================================================*/

void BSP_UART_Init(void);                           // UART初始化函数
void BSP_UART_SendByte(uint8_t data);               // 发送单字节函数，data为要发送的数据
void BSP_UART_SendString(const char *str);          // 发送字符串函数
void BSP_UART_SendData(uint8_t *data, uint16_t len);// 发送数据函数，data为数据指针，len为数据长度
uint8_t BSP_UART_ReceiveByte(void);                 // 接收单字节函数，返回接收到的数据
void BSP_UART_Printf(const char *fmt, ...);         // UART printf函数，支持格式化输出，fmt为格式字符串，...为可变参数列表

#if BSP_UART_USE_DMA
/**
 * @brief DMA传输完成回调
 * @note  需要在HAL_UART_TxCpltCallback中调用
 */
void BSP_UART_DMA_TxCpltCallback(void *huart);

/**
 * @brief 检查UART DMA是否忙
 */
uint8_t BSP_UART_IsBusy(void);
#endif

#ifdef __cplusplus
}
#endif

#endif /* __BSP_UART_H */
