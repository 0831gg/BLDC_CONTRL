#include "vofa_port.h"
#include <string.h>

/*=============================================================================
 *  VOFA+ 硬件抽象层 (支持DMA和阻塞两种模式)
 *
 *  波特率由 CubeMX 配置
 *  DMA模式：非阻塞发送，提高实时性
 *  阻塞模式：简单可靠，适合低速场景
 *=============================================================================*/

/* 发送错误计数（可选，用于调试） */
static volatile uint32_t s_tx_error_count = 0;

#if VOFA_USE_DMA
/* DMA发送状态标志 */
static volatile uint8_t s_tx_busy = 0;

/* DMA发送缓冲区（必须是静态或全局变量，DMA传输期间不能释放） */
static uint8_t s_dma_tx_buf[VOFA_TX_BUF_SIZE];
#endif

void VOFA_Port_SendData(const uint8_t* data, uint16_t len)
{
    HAL_StatusTypeDef status;

    if (data == NULL || len == 0) return;
    if (len > VOFA_TX_BUF_SIZE) len = VOFA_TX_BUF_SIZE;

#if VOFA_USE_DMA
    uint32_t timeout_start = HAL_GetTick();

    /* 等待上次DMA传输完成 */
    while (s_tx_busy) {
        if ((HAL_GetTick() - timeout_start) > VOFA_DMA_TIMEOUT_MS) {
            /* 超时，丢弃当前数据包 */
            s_tx_error_count++;
            return;
        }
    }

    /* 复制数据到DMA缓冲区（DMA传输期间源数据不能被修改） */
    memcpy(s_dma_tx_buf, data, len);

    /* 标记DMA忙 */
    s_tx_busy = 1;

    /* 启动DMA传输 */
    status = HAL_UART_Transmit_DMA(&VOFA_HUART, s_dma_tx_buf, len);

    if (status != HAL_OK) {
        /* DMA启动失败 */
        s_tx_busy = 0;
        s_tx_error_count++;
    }
#else
    /* 阻塞发送 */
    status = HAL_UART_Transmit(&VOFA_HUART, (uint8_t*)data, len, 100);

    if (status != HAL_OK) {
        /* 发送失败处理 */
        s_tx_error_count++;
    }
#endif
}

#if VOFA_USE_DMA
/**
 * @brief  DMA传输完成回调函数
 * @note   需要在stm32g4xx_it.c或main.c中调用此函数
 *         在HAL_UART_TxCpltCallback中调用
 */
void VOFA_Port_DMA_TxCpltCallback(UART_HandleTypeDef *huart)
{
    if (huart->Instance == VOFA_HUART.Instance) {
        s_tx_busy = 0;
    }
}

/**
 * @brief  检查DMA是否忙
 * @return 1=忙, 0=空闲
 */
uint8_t VOFA_Port_IsBusy(void)
{
    return s_tx_busy;
}
#endif

/**
 * @brief  获取发送错误计数（可选，用于调试）
 * @return 错误次数
 */
uint32_t VOFA_Port_GetErrorCount(void)
{
    return s_tx_error_count;
}

/**
 * @brief  清除错误计数（可选，用于调试）
 */
void VOFA_Port_ClearErrorCount(void)
{
    s_tx_error_count = 0;
}


