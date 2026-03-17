/**
 * @file    bsp_uart.c
 * @brief   USART1 shared transport implementation
 * @details Provides one reusable transmit backend for both BSP UART helpers
 *          and VOFA. The transmit mode is selected once in bsp_uart.h.
 */

#include "bsp_uart.h"
#include "usart.h"

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

static uint8_t uart_tx_buf[256];

#if BSP_UART_USE_DMA
static volatile uint8_t s_uart_tx_busy = 0U;
static uint8_t s_dma_tx_buf[256];
#endif

static uint32_t bsp_uart_normalize_timeout(uint32_t timeout_ms)
{
    return (timeout_ms == 0U) ? BSP_UART_TX_TIMEOUT_MS : timeout_ms;
}

static HAL_StatusTypeDef bsp_uart_transmit_blocking(const uint8_t *data, uint16_t len, uint32_t timeout_ms)
{
    return HAL_UART_Transmit(&huart1, (uint8_t *)data, len, bsp_uart_normalize_timeout(timeout_ms));
}

#if BSP_UART_USE_DMA
static HAL_StatusTypeDef bsp_uart_transmit_dma(const uint8_t *data, uint16_t len, uint32_t timeout_ms)
{
    HAL_StatusTypeDef status;
    uint32_t timeout_start = HAL_GetTick();
    uint32_t wait_timeout = bsp_uart_normalize_timeout(timeout_ms);

    while (s_uart_tx_busy) {
        if ((HAL_GetTick() - timeout_start) > wait_timeout) {
            return HAL_TIMEOUT;
        }
    }

    if (len > sizeof(s_dma_tx_buf)) {
        len = (uint16_t)sizeof(s_dma_tx_buf);
    }

    memcpy(s_dma_tx_buf, data, len);
    s_uart_tx_busy = 1U;

    status = HAL_UART_Transmit_DMA(&huart1, s_dma_tx_buf, len);
    if (status != HAL_OK) {
        s_uart_tx_busy = 0U;
    }

    return status;
}
#endif

void BSP_UART_Init(void)
{
    /* CubeMX already initializes USART1 in MX_USART1_UART_Init(). */
}

HAL_StatusTypeDef BSP_UART_Transmit(const uint8_t *data, uint16_t len, uint32_t timeout_ms)
{
    if (data == NULL || len == 0U) {
        return HAL_ERROR;
    }

#if BSP_UART_USE_DMA
    /* If CubeMX keeps the TX DMA linked, use DMA. Otherwise fallback safely. */
    if (huart1.hdmatx != NULL) {
        return bsp_uart_transmit_dma(data, len, timeout_ms);
    }
#endif

    return bsp_uart_transmit_blocking(data, len, timeout_ms);
}

void BSP_UART_SendByte(uint8_t data)
{
    (void)BSP_UART_Transmit(&data, 1U, 0U);
}

void BSP_UART_SendString(const char *str)
{
    uint16_t len;

    if (str == NULL) {
        return;
    }

    len = (uint16_t)strlen(str);
    if (len == 0U) {
        return;
    }

    (void)BSP_UART_Transmit((const uint8_t *)str, len, 0U);
}

void BSP_UART_SendData(uint8_t *data, uint16_t len)
{
    (void)BSP_UART_Transmit((const uint8_t *)data, len, 0U);
}

uint8_t BSP_UART_ReceiveByte(void)
{
    uint8_t data = 0;
    HAL_UART_Receive(&huart1, &data, 1, HAL_MAX_DELAY);
    return data;
}

void BSP_UART_Printf(const char *fmt, ...)
{
    va_list args;
    int len;

    if (fmt == NULL) {
        return;
    }

    va_start(args, fmt);
    len = vsnprintf((char *)uart_tx_buf, sizeof(uart_tx_buf), fmt, args);
    va_end(args);

    if (len <= 0) {
        return;
    }

    if ((size_t)len >= sizeof(uart_tx_buf)) {
        len = (int)(sizeof(uart_tx_buf) - 1U);
    }

    (void)BSP_UART_Transmit(uart_tx_buf, (uint16_t)len, 0U);
}

#if BSP_UART_USE_DMA
void BSP_UART_DMA_TxCpltCallback(UART_HandleTypeDef *huart)
{
    if (huart != NULL && huart->Instance == USART1) {
        s_uart_tx_busy = 0U;
    }
}
#endif

uint8_t BSP_UART_IsBusy(void)
{
#if BSP_UART_USE_DMA
    return s_uart_tx_busy;
#else
    return 0U;
#endif
}
