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

#endif /* __VOFA_PORT_CONFIG_H */
