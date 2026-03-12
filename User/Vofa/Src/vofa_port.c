#include "vofa_port.h"
#include <string.h>

/*=============================================================================
 *  VOFA+ 硬件抽象层实现
 *
 *  通过 VOFA_USE_HAL 宏自动选择 HAL库 / 标准库 实现
 *  通过 VOFA_PLATFORM_xxx 宏适配不同芯片系列
 *=============================================================================*/

/* =========================================================================
 *  波特率误差计算（所有平台通用）
 * ========================================================================= */
float VOFA_Port_CalcBaudError(uint32_t desired_baud, uint8_t over8)
{
    uint32_t pclk = VOFA_USART_PCLK_HZ;
    float divisor, actual_baud, error;
    uint32_t mantissa, fraction;
    uint8_t frac_bits;

    if (desired_baud == 0) return 100.0f;

    /* 不支持OVER8时，强制按OVER16计算 */
    if (over8 && !VOFA_SUPPORT_OVER8) return 100.0f;

    if (over8) {
        divisor   = (float)pclk / (8.0f * (float)desired_baud);
        frac_bits = 3;   /* OVER8:  BRR小数部分3位 */
    } else {
        divisor   = (float)pclk / (16.0f * (float)desired_baud);
        frac_bits = 4;   /* OVER16: BRR小数部分4位 */
    }

    mantissa = (uint32_t)divisor;
    fraction = (uint32_t)((divisor - (float)mantissa) * (float)(1u << frac_bits) + 0.5f);

    /* 小数部分溢出进位 */
    if (fraction >= (1u << frac_bits)) {
        mantissa++;
        fraction = 0;
    }

    if (mantissa == 0 && fraction == 0) return 100.0f;

    /* 反算实际波特率 */
    if (over8) {
        actual_baud = (float)pclk / (8.0f * ((float)mantissa + (float)fraction / 8.0f));
    } else {
        actual_baud = (float)pclk / (16.0f * ((float)mantissa + (float)fraction / 16.0f));
    }

    error = ((actual_baud - (float)desired_baud) / (float)desired_baud) * 100.0f;
    if (error < 0.0f) error = -error;

    return error;
}

uint8_t VOFA_Port_SupportOver8(void)
{
    return VOFA_SUPPORT_OVER8;
}

/* =========================================================================
 *  ██  HAL 库 实现
 * ========================================================================= */
#if VOFA_USE_HAL

/*--- DMA发送完成标志 ---*/
#if VOFA_USE_DMA
static volatile uint8_t vofa_dma_tx_cplt = 1;
#endif

void VOFA_Port_Init(uint32_t baudrate, uint8_t over8)
{
    UART_HandleTypeDef* huart = &VOFA_HUART;

    /*
     * CubeMX 已完成:
     *   - GPIO时钟使能、引脚复用配置 (HAL_UART_MspInit)
     *   - DMA通道初始化 (MX_DMA_Init)
     *   - USART基本初始化 (MX_USARTx_UART_Init)
     *
     * 这里只需用选定的波特率重新配置USART
     */
    HAL_UART_DeInit(huart);

    huart->Init.BaudRate     = baudrate;
    huart->Init.OverSampling = over8 ? UART_OVERSAMPLING_8 : UART_OVERSAMPLING_16;
    huart->Init.WordLength   = UART_WORDLENGTH_8B;
    huart->Init.StopBits     = UART_STOPBITS_1;
    huart->Init.Parity       = UART_PARITY_NONE;
    huart->Init.Mode         = UART_MODE_TX_RX;
    huart->Init.HwFlowCtl    = UART_HWCONTROL_NONE;

    HAL_UART_Init(huart);
}

void VOFA_Port_SendData(const uint8_t* data, uint16_t len)
{
    if (data == NULL || len == 0) return;

#if VOFA_USE_DMA
    /* 等待上次DMA完成 */
    while (!vofa_dma_tx_cplt);
    vofa_dma_tx_cplt = 0;
    HAL_UART_Transmit_DMA(&VOFA_HUART, (uint8_t*)data, len);
#else
    HAL_UART_Transmit(&VOFA_HUART, (uint8_t*)data, len, 100);
#endif
}

void VOFA_Port_WaitTxComplete(void)
{
#if VOFA_USE_DMA
    while (!vofa_dma_tx_cplt);
#endif
    /* 轮询模式下 HAL_UART_Transmit 本身就是阻塞的 */
}

/* HAL DMA发送完成回调 */
#if VOFA_USE_DMA
void HAL_UART_TxCpltCallback(UART_HandleTypeDef *huart)
{
    if (huart->Instance == VOFA_HUART.Instance) {
        vofa_dma_tx_cplt = 1;
    }
}
#endif /* VOFA_USE_DMA */


/* =========================================================================
 *  标准库 实现
 * ========================================================================= */
#else /* !VOFA_USE_HAL */

/*--- DMA发送状态 ---*/
#if VOFA_USE_DMA
static volatile uint8_t vofa_dma_busy = 0;
#endif

/* ---------- GPIO 初始化 ---------- */
static void vofa_gpio_init(void)
{
#ifdef VOFA_PLATFORM_STM32F4
    GPIO_InitTypeDef GPIO_InitStruct;

    VOFA_TX_GPIO_CLK_CMD(VOFA_TX_GPIO_CLK, ENABLE);
    VOFA_RX_GPIO_CLK_CMD(VOFA_RX_GPIO_CLK, ENABLE);

    GPIO_PinAFConfig(VOFA_TX_PORT, VOFA_TX_PINSOURCE, VOFA_TX_AF);
    GPIO_PinAFConfig(VOFA_RX_PORT, VOFA_RX_PINSOURCE, VOFA_RX_AF);

    GPIO_InitStruct.GPIO_Mode  = GPIO_Mode_AF;
    GPIO_InitStruct.GPIO_Speed = GPIO_Speed_100MHz;
    GPIO_InitStruct.GPIO_OType = GPIO_OType_PP;
    GPIO_InitStruct.GPIO_PuPd  = GPIO_PuPd_UP;

    GPIO_InitStruct.GPIO_Pin = VOFA_TX_PIN;
    GPIO_Init(VOFA_TX_PORT, &GPIO_InitStruct);

    GPIO_InitStruct.GPIO_Pin = VOFA_RX_PIN;
    GPIO_Init(VOFA_RX_PORT, &GPIO_InitStruct);

#elif defined(VOFA_PLATFORM_STM32F1)
    GPIO_InitTypeDef GPIO_InitStruct;

    VOFA_TX_GPIO_CLK_CMD(VOFA_TX_GPIO_CLK, ENABLE);
    VOFA_RX_GPIO_CLK_CMD(VOFA_RX_GPIO_CLK, ENABLE);

    /* TX: 复用推挽 */
    GPIO_InitStruct.GPIO_Pin   = VOFA_TX_PIN;
    GPIO_InitStruct.GPIO_Mode  = GPIO_Mode_AF_PP;
    GPIO_InitStruct.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(VOFA_TX_PORT, &GPIO_InitStruct);

    /* RX: 浮空输入 */
    GPIO_InitStruct.GPIO_Pin  = VOFA_RX_PIN;
    GPIO_InitStruct.GPIO_Mode = GPIO_Mode_IN_FLOATING;
    GPIO_Init(VOFA_RX_PORT, &GPIO_InitStruct);
#endif
}

/* ---------- USART 初始化 ---------- */
static void vofa_usart_init(uint32_t baudrate, uint8_t over8)
{
    USART_InitTypeDef USART_InitStruct;

    VOFA_USART_CLK_CMD(VOFA_USART_CLK, ENABLE);
    USART_Cmd(VOFA_USARTx, DISABLE);

    /* 设置过采样模式 */
#if VOFA_SUPPORT_OVER8
    if (over8) {
        USART_OverSampling8Cmd(VOFA_USARTx, ENABLE);
    } else {
        USART_OverSampling8Cmd(VOFA_USARTx, DISABLE);
    }
#else
    (void)over8;
#endif

    USART_InitStruct.USART_BaudRate            = baudrate;
    USART_InitStruct.USART_WordLength          = USART_WordLength_8b;
    USART_InitStruct.USART_StopBits            = USART_StopBits_1;
    USART_InitStruct.USART_Parity              = USART_Parity_No;
    USART_InitStruct.USART_HardwareFlowControl = USART_HardwareFlowControl_None;
    USART_InitStruct.USART_Mode                = USART_Mode_Tx | USART_Mode_Rx;
    USART_Init(VOFA_USARTx, &USART_InitStruct);

    USART_Cmd(VOFA_USARTx, ENABLE);
}

/* ---------- DMA 初始化（标准库）---------- */
#if VOFA_USE_DMA && defined(VOFA_PLATFORM_STM32F4)
static void vofa_dma_init(void)
{
    DMA_InitTypeDef DMA_InitStruct;

    VOFA_DMA_CLK_CMD(VOFA_DMA_CLK, ENABLE);
    DMA_DeInit(VOFA_DMA_STREAM);

    DMA_InitStruct.DMA_Channel            = VOFA_DMA_CHANNEL;
    DMA_InitStruct.DMA_PeripheralBaseAddr = (uint32_t)&(VOFA_USARTx->DR);
    DMA_InitStruct.DMA_Memory0BaseAddr    = 0;
    DMA_InitStruct.DMA_DIR                = DMA_DIR_MemoryToPeripheral;
    DMA_InitStruct.DMA_BufferSize         = 1;
    DMA_InitStruct.DMA_PeripheralInc      = DMA_PeripheralInc_Disable;
    DMA_InitStruct.DMA_MemoryInc          = DMA_MemoryInc_Enable;
    DMA_InitStruct.DMA_PeripheralDataSize = DMA_PeripheralDataSize_Byte;
    DMA_InitStruct.DMA_MemoryDataSize     = DMA_MemoryDataSize_Byte;
    DMA_InitStruct.DMA_Mode               = DMA_Mode_Normal;
    DMA_InitStruct.DMA_Priority           = DMA_Priority_Medium;
    DMA_InitStruct.DMA_FIFOMode           = DMA_FIFOMode_Disable;
    DMA_Init(VOFA_DMA_STREAM, &DMA_InitStruct);

    USART_DMACmd(VOFA_USARTx, USART_DMAReq_Tx, ENABLE);
}
#endif

/* ---------- Port 接口实现 ---------- */

void VOFA_Port_Init(uint32_t baudrate, uint8_t over8)
{
    vofa_gpio_init();
    vofa_usart_init(baudrate, over8);

#if VOFA_USE_DMA && defined(VOFA_PLATFORM_STM32F4)
    vofa_dma_init();
#endif
}

void VOFA_Port_SendData(const uint8_t* data, uint16_t len)
{
    uint16_t i;
    if (data == NULL || len == 0) return;

#if VOFA_USE_DMA && defined(VOFA_PLATFORM_STM32F4)
    /* 等待上一次DMA完成 */
    while (vofa_dma_busy) {
        if (DMA_GetFlagStatus(VOFA_DMA_STREAM, VOFA_DMA_FLAG_TC)) {
            DMA_ClearFlag(VOFA_DMA_STREAM, VOFA_DMA_FLAG_TC);
            DMA_Cmd(VOFA_DMA_STREAM, DISABLE);
            vofa_dma_busy = 0;
        }
    }
    DMA_Cmd(VOFA_DMA_STREAM, DISABLE);
    DMA_ClearFlag(VOFA_DMA_STREAM, VOFA_DMA_FLAG_TC);
    VOFA_DMA_STREAM->NDTR = len;
    VOFA_DMA_STREAM->M0AR = (uint32_t)data;
    DMA_Cmd(VOFA_DMA_STREAM, ENABLE);
    vofa_dma_busy = 1;
#else
    /* 轮询发送 */
    for (i = 0; i < len; i++) {
        while (USART_GetFlagStatus(VOFA_USARTx, USART_FLAG_TXE) == RESET);
        USART_SendData(VOFA_USARTx, data[i]);
    }
    while (USART_GetFlagStatus(VOFA_USARTx, USART_FLAG_TC) == RESET);
#endif
}

void VOFA_Port_WaitTxComplete(void)
{
#if VOFA_USE_DMA && defined(VOFA_PLATFORM_STM32F4)
    while (vofa_dma_busy) {
        if (DMA_GetFlagStatus(VOFA_DMA_STREAM, VOFA_DMA_FLAG_TC)) {
            DMA_ClearFlag(VOFA_DMA_STREAM, VOFA_DMA_FLAG_TC);
            DMA_Cmd(VOFA_DMA_STREAM, DISABLE);
            vofa_dma_busy = 0;
        }
    }
#else
    while (USART_GetFlagStatus(VOFA_USARTx, USART_FLAG_TC) == RESET);
#endif
}

#endif /* !VOFA_USE_HAL */
