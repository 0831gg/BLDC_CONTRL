#ifndef __VOFA_PORT_CONFIG_H
#define __VOFA_PORT_CONFIG_H

/*=============================================================================
 *  VOFA+ transport configuration
 *
 *  The UART instance remains configurable here.
 *  The transmit mode itself is shared with BSP UART and is configured only in
 *  bsp_uart.h through BSP_UART_TX_MODE.
 *=============================================================================*/

#include "main.h"
#include "usart.h"
#include "bsp_uart.h"

/* UART handle used by VOFA. */
#define VOFA_HUART              huart1

/* Maximum VOFA frame buffer size in bytes. */
#define VOFA_TX_BUF_SIZE        256

/* Compatibility aliases. VOFA follows the shared BSP UART transport mode. */
#define VOFA_USE_DMA            BSP_UART_USE_DMA
#define VOFA_DMA_TIMEOUT_MS     BSP_UART_TX_TIMEOUT_MS

#endif /* __VOFA_PORT_CONFIG_H */
