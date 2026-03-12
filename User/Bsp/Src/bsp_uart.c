#include "bsp_uart.h"
#include "usart.h"
#include <stdio.h>
#include <string.h>
#include <stdarg.h>

static uint8_t uart_tx_buf[256];

void BSP_UART_Init(void)
{
    /* CubeMX 已完成 MX_USART1_UART_Init(), 无需额外配置 */
}

void BSP_UART_SendByte(uint8_t data)
{
    HAL_UART_Transmit(&huart1, &data, 1, HAL_MAX_DELAY);
}

void BSP_UART_SendString(const char *str)
{
    if (str == NULL) return;
    HAL_UART_Transmit(&huart1, (uint8_t *)str, strlen(str), HAL_MAX_DELAY);
}

void BSP_UART_SendData(uint8_t *data, uint16_t len)
{
    if (data == NULL || len == 0) return;
    HAL_UART_Transmit(&huart1, data, len, HAL_MAX_DELAY);
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

    va_start(args, fmt);
    len = vsnprintf((char *)uart_tx_buf, sizeof(uart_tx_buf), fmt, args);
    va_end(args);

    if (len > 0) {
        if ((uint16_t)len > sizeof(uart_tx_buf))
            len = sizeof(uart_tx_buf);
        HAL_UART_Transmit(&huart1, uart_tx_buf, (uint16_t)len, HAL_MAX_DELAY);
    }
}
