#ifndef __VOFA_UART_H
#define __VOFA_UART_H

#include "vofa_port.h"
#include <stdint.h>
#include <stdarg.h>

/*=============================================================================
 *  VOFA+ protocol helpers
 *
 *  JustFloat: binary float stream + {0x00, 0x00, 0x80, 0x7F}
 *  FireWater: ASCII "ch0, ch1, ..., chN\n"
 *
 *  Only one protocol should be selected in VOFA+ at a time.
 *  The UART baud rate is defined by CubeMX. The current project setting is
 *  115200 baud on USART1.
 *=============================================================================*/

/**
 * @brief Initialize the VOFA helper state.
 * @note  This does not change the UART baud rate or peripheral registers.
 */
void VOFA_Init(void);

/**
 * @brief Send multi-channel float data using the JustFloat protocol.
 * @param data Pointer to the float array.
 * @param ch_count Number of channels to send.
 */
void VOFA_JustFloat(float* data, uint8_t ch_count);

/**
 * @brief Send formatted FireWater text.
 */
void VOFA_Printf(const char* fmt, ...);

/**
 * @brief Send multi-channel float data using the FireWater protocol.
 * @param data Pointer to the float array.
 * @param ch_count Number of channels to send.
 * @note  Output format: "v0, v1, ..., vN\n"
 */
void VOFA_FireWaterFloat(float* data, uint8_t ch_count);

/**
 * @brief Send raw bytes without protocol framing.
 */
void VOFA_SendRaw(const uint8_t* data, uint16_t len);

#endif /* __VOFA_UART_H */
