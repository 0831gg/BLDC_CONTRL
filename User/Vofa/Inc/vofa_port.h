#ifndef __VOFA_PORT_H
#define __VOFA_PORT_H

#include "vofa_port_config.h"
#include <stdint.h>

/*=============================================================================
 *  VOFA+ transport abstraction
 *
 *  VOFA now reuses the shared USART1 transport implemented in bsp_uart.c.
 *  This means BSP and VOFA always follow the same blocking/DMA transmit mode
 *  selected in bsp_uart.h.
 *=============================================================================*/

/**
 * @brief  Send one VOFA payload.
 * @param  data Pointer to the payload bytes.
 * @param  len  Payload length in bytes. Max recommended size: 256 bytes.
 */
void VOFA_Port_SendData(const uint8_t* data, uint16_t len);

/**
 * @brief  Check whether the shared USART1 transport is busy.
 * @retval 1 when a DMA transfer is in progress, otherwise 0.
 */
uint8_t VOFA_Port_IsBusy(void);

/**
 * @brief  Get the accumulated VOFA transmit error count.
 * @retval Total number of send errors since startup or the last clear.
 */
uint32_t VOFA_Port_GetErrorCount(void);

/**
 * @brief  Clear the VOFA transmit error counter.
 */
void VOFA_Port_ClearErrorCount(void);

#endif /* __VOFA_PORT_H */
