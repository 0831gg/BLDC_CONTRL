#ifndef __VOFA_PORT_H
#define __VOFA_PORT_H

#include "vofa_port_config.h"
#include <stdint.h>

/*=============================================================================
 *  VOFA+ 硬件抽象层接口
 *
 *  协议层(vofa_uart.c)只调用以下函数，不直接操作任何硬件寄存器
 *  支持 HAL库 / 标准库 自动切换
 *=============================================================================*/

/**
 * @brief  初始化USART硬件（GPIO + USART外设）
 * @param  baudrate: 波特率
 * @param  over8:    过采样模式 1=OVER8, 0=OVER16
 */
void VOFA_Port_Init(uint32_t baudrate, uint8_t over8);

/**
 * @brief  发送数据块
 * @param  data: 数据指针
 * @param  len:  数据长度
 * @note   DMA模式下非阻塞，轮询模式下阻塞
 */
void VOFA_Port_SendData(const uint8_t* data, uint16_t len);

/**
 * @brief  等待当前发送完成（阻塞）
 */
void VOFA_Port_WaitTxComplete(void);

/**
 * @brief  计算指定波特率在当前硬件上的误差
 * @param  desired_baud: 期望波特率
 * @param  over8:        过采样模式
 * @retval 误差百分比（绝对值），100.0 = 不可实现
 */
float VOFA_Port_CalcBaudError(uint32_t desired_baud, uint8_t over8);

/**
 * @brief  查询硬件是否支持OVER8过采样
 * @retval 1=支持, 0=不支持
 */
uint8_t VOFA_Port_SupportOver8(void);

#endif /* __VOFA_PORT_H */

