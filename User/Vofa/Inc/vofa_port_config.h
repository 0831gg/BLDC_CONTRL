#ifndef __VOFA_PORT_CONFIG_H
#define __VOFA_PORT_CONFIG_H

/*=============================================================================
 *  VOFA+ 移植配置 (HAL 精简版)
 *
 *  波特率由 CubeMX 配置 USART 外设决定, 本模块不做运行时修改
 *  移植时只需修改 VOFA_HUART 和 VOFA_TX_BUF_SIZE
 *=============================================================================*/

#include "main.h"
#include "usart.h"

/* 使用的 UART 句柄 (CubeMX 生成, 见 usart.h) */
#define VOFA_HUART              huart1

/* 发送缓冲区大小(字节), 需 >= 最大单帧长度 */
#define VOFA_TX_BUF_SIZE        256

/*=============================================================================
 *  DMA 配置
 *
 *  使用DMA发送可以避免阻塞主循环，提高实时性
 *  注意：使用DMA前需要在CubeMX中配置USART1的DMA TX
 *=============================================================================*/

/**
 * @brief 是否使用DMA发送
 * @note  0 = 阻塞发送 (HAL_UART_Transmit)
 *        1 = DMA发送 (HAL_UART_Transmit_DMA)
 * @warning 使用DMA前必须在CubeMX中配置USART1 DMA TX:
 *          1. DMA Settings → Add → USART1_TX
 *          2. Mode: Normal
 *          3. Priority: Low/Medium
 *          4. Data Width: Byte
 */
#define VOFA_USE_DMA            1

/**
 * @brief DMA发送超时时间(ms)
 * @note  当DMA忙时，等待的最大时间
 *        超时后会丢弃当前数据包
 */
#define VOFA_DMA_TIMEOUT_MS     10

#endif /* __VOFA_PORT_CONFIG_H */
