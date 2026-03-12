#include "vofa_uart.h"
#include <string.h>
#include <stdio.h>
#include <math.h>

/*=============================================================================
 *  VOFA+ 协议层实现
 *
 *  ★ 本文件不包含任何硬件相关代码 ★
 *  所有硬件操作均通过 vofa_port.h 接口完成
 *=============================================================================*/

/* JustFloat帧尾: IEEE754 NaN的小端表示 */
static const uint8_t JUSTFLOAT_TAIL[4] = {0x00, 0x00, 0x80, 0x7F};

/* 波特率降级表 */
static const uint32_t baud_table[VOFA_BAUD_TABLE_SIZE] = VOFA_BAUD_TABLE;

/* VOFA状态 */
static VOFA_Info_t vofa_info;

/* 发送缓冲区（DMA和轮询共用） */
static uint8_t tx_buf[VOFA_TX_BUF_SIZE];

/* JustFloat专用缓冲区（DMA模式下防止数据覆盖） */
#if VOFA_USE_DMA
static uint8_t jf_buf[VOFA_TX_BUF_SIZE];
#endif

/*========================== 波特率自动选择 ==========================*/

/**
 * @brief  扫描波特率表，选择误差在允许范围内的最高波特率
 *
 *  扫描策略（每个波特率）:
 *    1. 优先尝试 OVER8（能达到更高波特率）
 *    2. 若不支持或误差过大，降级到 OVER16
 *    3. 仍不行则尝试下一个更低的波特率
 */
static uint32_t vofa_auto_select(uint8_t* out_over8, float* out_error)
{
    uint8_t i;
    float err;
    uint8_t sup8 = VOFA_Port_SupportOver8();

    for (i = 0; i < VOFA_BAUD_TABLE_SIZE; i++) {
        uint32_t baud = baud_table[i];

        /* 尝试 OVER8 */
        if (sup8) {
            err = VOFA_Port_CalcBaudError(baud, 1);
            if (err <= VOFA_MAX_BAUD_ERROR) {
                *out_over8 = 1;
                *out_error = err;
                vofa_info.baud_index = i;
                return baud;
            }
        }

        /* 尝试 OVER16 */
        err = VOFA_Port_CalcBaudError(baud, 0);
        if (err <= VOFA_MAX_BAUD_ERROR) {
            *out_over8 = 0;
            *out_error = err;
            vofa_info.baud_index = i;
            return baud;
        }
    }

    /* 全部失败 → 使用最低档位 */
    *out_over8 = 0;
    *out_error = VOFA_Port_CalcBaudError(baud_table[VOFA_BAUD_TABLE_SIZE - 1], 0);
    vofa_info.baud_index = VOFA_BAUD_TABLE_SIZE - 1;
    return baud_table[VOFA_BAUD_TABLE_SIZE - 1];
}

/*========================== 公共接口 ==========================*/

void VOFA_Init(void)
{
    uint32_t baud;
    float err = 0;

    memset(&vofa_info, 0, sizeof(vofa_info));

    /* 自动选择最优波特率 */
    baud = vofa_auto_select(&vofa_info.over8, &err);
    vofa_info.baudrate   = baud;
    vofa_info.baud_error = err;

    /* 初始化硬件 */
    VOFA_Port_Init(baud, vofa_info.over8);
    vofa_info.initialized = 1;

    /* 打印启动信息（用FireWater协议，VOFA+可直接显示文本） */
    VOFA_Printf("================================\r\n");
    VOFA_Printf("[VOFA] Init OK\r\n");
    VOFA_Printf("[VOFA] Baudrate : %lu\r\n", (unsigned long)baud);
    VOFA_Printf("[VOFA] Mode     : %s\r\n", vofa_info.over8 ? "OVER8" : "OVER16");
    VOFA_Printf("[VOFA] Error    : %.2f%%\r\n", err);
    VOFA_Printf("[VOFA] PCLK     : %lu Hz\r\n", (unsigned long)VOFA_USART_PCLK_HZ);
    VOFA_Printf("[VOFA] Library  : %s\r\n", VOFA_USE_HAL ? "HAL" : "StdPeriph");
    VOFA_Printf("================================\r\n");
}

void VOFA_InitWithBaud(uint32_t baudrate, uint8_t over8)
{
    memset(&vofa_info, 0, sizeof(vofa_info));

    vofa_info.baudrate   = baudrate;
    vofa_info.over8      = over8;
    vofa_info.baud_error = VOFA_Port_CalcBaudError(baudrate, over8);

    VOFA_Port_Init(baudrate, over8);
    vofa_info.initialized = 1;

    VOFA_Printf("[VOFA] Force Baud: %lu (%s) Err:%.2f%%\r\n",
                (unsigned long)baudrate,
                over8 ? "OVER8" : "OVER16",
                vofa_info.baud_error);
}

uint32_t VOFA_GetBaudrate(void)
{
    return vofa_info.baudrate;
}

const VOFA_Info_t* VOFA_GetInfo(void)
{
    return &vofa_info;
}

void VOFA_JustFloat(float* data, uint8_t ch_count)
{
    uint16_t data_len;

    if (!vofa_info.initialized || data == NULL || ch_count == 0) return;

    data_len = ch_count * (uint16_t)sizeof(float);
    if (data_len + 4u > VOFA_TX_BUF_SIZE) return;

#if VOFA_USE_DMA
    /* DMA: 拷贝到专用缓冲区，等待上帧完成后发送 */
    VOFA_Port_WaitTxComplete();
    memcpy(jf_buf, data, data_len);
    memcpy(jf_buf + data_len, JUSTFLOAT_TAIL, 4);
    VOFA_Port_SendData(jf_buf, data_len + 4u);
#else
    /* 轮询: 直接发送 */
    VOFA_Port_SendData((const uint8_t*)data, data_len);
    VOFA_Port_SendData(JUSTFLOAT_TAIL, 4);
#endif
}

void VOFA_Printf(const char* fmt, ...)
{
    va_list args;
    int len;

    if (!vofa_info.initialized) return;

#if VOFA_USE_DMA
    VOFA_Port_WaitTxComplete();
#endif

    va_start(args, fmt);
    len = vsnprintf((char*)tx_buf, VOFA_TX_BUF_SIZE, fmt, args);
    va_end(args);

    if (len > 0) {
        if ((uint16_t)len > VOFA_TX_BUF_SIZE) len = VOFA_TX_BUF_SIZE;
        VOFA_Port_SendData(tx_buf, (uint16_t)len);
    }
}

void VOFA_FireWaterFloat(float* data, uint8_t ch_count)
{
    int offset = 0, ret;
    uint8_t i;

    if (!vofa_info.initialized || data == NULL || ch_count == 0) return;

#if VOFA_USE_DMA
    VOFA_Port_WaitTxComplete();
#endif

    for (i = 0; i < ch_count; i++) {
        if (i > 0) {
            ret = snprintf((char*)tx_buf + offset,
                           VOFA_TX_BUF_SIZE - (uint16_t)offset, ",");
            if (ret > 0) offset += ret;
        }
        ret = snprintf((char*)tx_buf + offset,
                       VOFA_TX_BUF_SIZE - (uint16_t)offset, "%.4f", data[i]);
        if (ret > 0) offset += ret;
    }

    /* 换行符作为帧结束 */
    ret = snprintf((char*)tx_buf + offset,
                   VOFA_TX_BUF_SIZE - (uint16_t)offset, "\n");
    if (ret > 0) offset += ret;

    VOFA_Port_SendData(tx_buf, (uint16_t)offset);
}

void VOFA_SendRaw(const uint8_t* data, uint16_t len)
{
    if (!vofa_info.initialized || data == NULL || len == 0) return;

#if VOFA_USE_DMA
    VOFA_Port_WaitTxComplete();
#endif

    VOFA_Port_SendData(data, len);
}
