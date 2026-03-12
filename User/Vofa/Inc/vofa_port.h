#ifndef __VOFA_PORT_H
#define __VOFA_PORT_H

#include "vofa_port_config.h"
#include <stdint.h>

/*=============================================================================
 *  VOFA+ 硬件抽象层
 *
 *  波特率由 CubeMX 配置, 此层只负责发送数据
 *=============================================================================*/

/**
 * @brief  发送数据块 (阻塞)
 * @param  data: 数据指针
 * @param  len:  数据长度
 */
void VOFA_Port_SendData(const uint8_t* data, uint16_t len);

#endif /* __VOFA_PORT_H */

