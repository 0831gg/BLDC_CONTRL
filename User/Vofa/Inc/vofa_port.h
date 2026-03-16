#ifndef __VOFA_PORT_H
#define __VOFA_PORT_H

#include "vofa_port_config.h"
#include <stdint.h>

/*=============================================================================
 *  VOFA+ 硬件抽象层
 *
 *  功能：提供UART发送接口，支持DMA和阻塞两种模式
 *  配置：通过 vofa_port_config.h 中的 VOFA_USE_DMA 切换模式
 *  波特率：由 CubeMX 配置 USART 外设决定
 *
 *  DMA模式（VOFA_USE_DMA=1）：
 *    - 非阻塞发送，CPU不等待
 *    - 数据自动复制到内部DMA缓冲区
 *    - 需要在CubeMX中配置USART1 DMA TX
 *    - 需要在HAL_UART_TxCpltCallback中调用VOFA_Port_DMA_TxCpltCallback
 *
 *  阻塞模式（VOFA_USE_DMA=0）：
 *    - 阻塞发送，等待数据发送完成
 *    - 简单可靠，无需额外配置
 *    - 适合低速场景
 *=============================================================================*/

/**
 * @brief  发送数据块
 * @param  data: 数据指针
 * @param  len:  数据长度（最大256字节）
 * @retval 无
 *
 * @note   DMA模式（VOFA_USE_DMA=1）：
 *         - 非阻塞，立即返回（<0.1ms）
 *         - 数据会被复制到内部DMA缓冲区
 *         - 如果DMA忙，会等待最多VOFA_DMA_TIMEOUT_MS
 *         - 超时后丢弃数据，错误计数+1
 *
 * @note   阻塞模式（VOFA_USE_DMA=0）：
 *         - 阻塞直到发送完成
 *         - 发送时间 = 数据长度 × 10 / 波特率
 *         - 例如：256字节 @ 115200bps ≈ 22ms
 *
 * @note   发送失败时会记录错误计数，可通过VOFA_Port_GetErrorCount()查询
 */
void VOFA_Port_SendData(const uint8_t* data, uint16_t len);

#if VOFA_USE_DMA
/**
 * @brief  DMA传输完成回调函数
 * @param  huart: UART句柄指针
 * @retval 无
 *
 * @note   此函数必须在HAL_UART_TxCpltCallback中调用
 *
 * @note   使用示例（在usart.c或main.c中添加）：
 *         @code
 *         void HAL_UART_TxCpltCallback(UART_HandleTypeDef *huart)
 *         {
 *             VOFA_Port_DMA_TxCpltCallback(huart);
 *         }
 *         @endcode
 *
 * @warning 如果忘记添加此回调，DMA会一直处于忙状态，导致：
 *          - 第一次发送成功
 *          - 后续发送全部超时失败
 *          - 错误计数持续增加
 */
void VOFA_Port_DMA_TxCpltCallback(UART_HandleTypeDef *huart);

/**
 * @brief  检查DMA是否忙
 * @retval 1=忙（正在传输），0=空闲
 *
 * @note   可用于监控DMA状态或实现流控
 *
 * @note   使用示例：
 *         @code
 *         if (!VOFA_Port_IsBusy()) {
 *             // DMA空闲，可以发送新数据
 *             VOFA_Port_SendData(data, len);
 *         }
 *         @endcode
 */
uint8_t VOFA_Port_IsBusy(void);
#endif

/**
 * @brief  获取发送错误计数
 * @retval 错误次数（累计）
 *
 * @note   错误类型包括：
 *         - HAL_UART_Transmit 失败（阻塞模式）
 *         - HAL_UART_Transmit_DMA 失败（DMA模式）
 *         - DMA超时（等待DMA空闲超时）
 *
 * @note   用于调试和监控，建议定期检查：
 *         @code
 *         uint32_t errors = VOFA_Port_GetErrorCount();
 *         if (errors > 0) {
 *             // 发现错误，记录日志或触发告警
 *             VOFA_Port_ClearErrorCount();
 *         }
 *         @endcode
 */
uint32_t VOFA_Port_GetErrorCount(void);

/**
 * @brief  清除错误计数
 * @retval 无
 *
 * @note   用于重置错误统计，通常在处理完错误后调用
 */
void VOFA_Port_ClearErrorCount(void);

#endif /* __VOFA_PORT_H */

