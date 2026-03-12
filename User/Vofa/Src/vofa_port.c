#include "vofa_port.h"

/*=============================================================================
 *  VOFA+ 硬件抽象层 (HAL 精简版)
 *
 *  波特率由 CubeMX 配置, 此处仅负责阻塞发送
 *=============================================================================*/

void VOFA_Port_SendData(const uint8_t* data, uint16_t len)
{
    if (data == NULL || len == 0) return;
    HAL_UART_Transmit(&VOFA_HUART, (uint8_t*)data, len, 100);
}


