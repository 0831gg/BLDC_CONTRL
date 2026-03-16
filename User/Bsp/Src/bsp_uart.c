/**
 * @file    bsp_uart.c
 * @brief   UART调试通信实现
 * @details 实现UART1的各种数据发送功能
 *          支持DMA和阻塞两种发送模式
 * @see     bsp_uart.h
 */

#include "bsp_uart.h"
#include "usart.h"
#include <stdio.h>
#include <string.h>
#include <stdarg.h>

/** @brief UART发送缓冲区 */
static uint8_t uart_tx_buf[256];

#if BSP_UART_USE_DMA
/** @brief DMA发送状态标志 */
static volatile uint8_t s_uart_tx_busy = 0;

/** @brief DMA发送缓冲区（必须是静态变量） */
static uint8_t s_dma_tx_buf[256];
#endif

/**
 * @brief 初始化UART
 * @note USART1已在CubeMX中配置好，这里无需额外初始化
 */
void BSP_UART_Init(void)
{
    /* CubeMX 已完成 MX_USART1_UART_Init(), 无需额外配置 */
}

/**
 * @brief 发送单字节数据
 * @param data 要发送的字节
 */
void BSP_UART_SendByte(uint8_t data)
{
#if BSP_UART_USE_DMA
    uint32_t timeout_start = HAL_GetTick();

    /* 等待DMA空闲 */
    while (s_uart_tx_busy) {
        if ((HAL_GetTick() - timeout_start) > BSP_UART_DMA_TIMEOUT_MS) {
            return;  /* 超时，丢弃数据 */
        }
    }

    s_dma_tx_buf[0] = data;
    s_uart_tx_busy = 1;
    HAL_UART_Transmit_DMA(&huart1, s_dma_tx_buf, 1);
#else
    HAL_UART_Transmit(&huart1, &data, 1, HAL_MAX_DELAY);
#endif
}

/**
 * @brief 发送字符串
 * @param str 字符串指针
 * @note 发送NULL结尾的字符串
 */
void BSP_UART_SendString(const char *str)
{
    uint16_t len;

    if (str == NULL) return;

    len = strlen(str);
    if (len == 0) return;

#if BSP_UART_USE_DMA
    uint32_t timeout_start = HAL_GetTick();

    /* 等待DMA空闲 */
    while (s_uart_tx_busy) {
        if ((HAL_GetTick() - timeout_start) > BSP_UART_DMA_TIMEOUT_MS) {
            return;  /* 超时，丢弃数据 */
        }
    }

    if (len > sizeof(s_dma_tx_buf)) len = sizeof(s_dma_tx_buf);
    memcpy(s_dma_tx_buf, str, len);
    s_uart_tx_busy = 1;
    HAL_UART_Transmit_DMA(&huart1, s_dma_tx_buf, len);
#else
    HAL_UART_Transmit(&huart1, (uint8_t *)str, len, HAL_MAX_DELAY);
#endif
}

/**
 * @brief 发送数据块
 * @param data 数据指针
 * @param len 数据长度
 */
void BSP_UART_SendData(uint8_t *data, uint16_t len)
{
    if (data == NULL || len == 0) return;

#if BSP_UART_USE_DMA
    uint32_t timeout_start = HAL_GetTick();

    /* 等待DMA空闲 */
    while (s_uart_tx_busy) {
        if ((HAL_GetTick() - timeout_start) > BSP_UART_DMA_TIMEOUT_MS) {
            return;  /* 超时，丢弃数据 */
        }
    }

    if (len > sizeof(s_dma_tx_buf)) len = sizeof(s_dma_tx_buf);
    memcpy(s_dma_tx_buf, data, len);
    s_uart_tx_busy = 1;
    HAL_UART_Transmit_DMA(&huart1, s_dma_tx_buf, len);
#else
    HAL_UART_Transmit(&huart1, data, len, HAL_MAX_DELAY);
#endif
}

/**
 * @brief 接收单字节（阻塞）
 * @return uint8_t 接收到的数据
 */
uint8_t BSP_UART_ReceiveByte(void)
{
    uint8_t data = 0;
    HAL_UART_Receive(&huart1, &data, 1, HAL_MAX_DELAY);
    return data;
}

/**
 * @brief 格式化打印（类似printf）
 * @param fmt 格式字符串
 * @param ... 可变参数
 * @note 支持常见的printf格式符: %d, %u, %x, %f, %s, %c
 */
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

#if BSP_UART_USE_DMA
        uint32_t timeout_start = HAL_GetTick();

        /* 等待DMA空闲 */
        while (s_uart_tx_busy) {
            if ((HAL_GetTick() - timeout_start) > BSP_UART_DMA_TIMEOUT_MS) {
                return;  /* 超时，丢弃数据 */
            }
        }

        memcpy(s_dma_tx_buf, uart_tx_buf, (uint16_t)len);
        s_uart_tx_busy = 1;
        HAL_UART_Transmit_DMA(&huart1, s_dma_tx_buf, (uint16_t)len);
#else
        HAL_UART_Transmit(&huart1, uart_tx_buf, (uint16_t)len, HAL_MAX_DELAY);
#endif
    }
}

#if BSP_UART_USE_DMA
/**
 * @brief DMA传输完成回调
 * @note  需要在HAL_UART_TxCpltCallback中调用
 */
void BSP_UART_DMA_TxCpltCallback(void *huart)
{
    UART_HandleTypeDef *uart_handle = (UART_HandleTypeDef *)huart;

    if (uart_handle->Instance == USART1) {
        s_uart_tx_busy = 0;
    }
}

/**
 * @brief 检查UART DMA是否忙
 */
uint8_t BSP_UART_IsBusy(void)
{
    return s_uart_tx_busy;
}
#endif
