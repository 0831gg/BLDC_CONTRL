#include "vofa_uart.h"
#include <string.h>
#include <stdio.h>

/*=============================================================================
 *  VOFA+ 协议层 (精简版)
 *
 *  波特率由 CubeMX 配置, 此处只负责协议组帧
 *=============================================================================*/

/* JustFloat 帧尾: IEEE754 NaN 小端表示 */
static const uint8_t JUSTFLOAT_TAIL[4] = {0x00, 0x00, 0x80, 0x7F};

static uint8_t tx_buf[VOFA_TX_BUF_SIZE];
static uint8_t vofa_initialized = 0;

void VOFA_Init(void)
{
    vofa_initialized = 1;
}

void VOFA_JustFloat(float* data, uint8_t ch_count)
{
    if (!vofa_initialized || data == NULL || ch_count == 0) return;
    VOFA_Port_SendData((const uint8_t*)data, ch_count * (uint16_t)sizeof(float));
    VOFA_Port_SendData(JUSTFLOAT_TAIL, 4);
}

void VOFA_Printf(const char* fmt, ...)
{
    va_list args;
    int len;

    if (!vofa_initialized) return;

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

    if (!vofa_initialized || data == NULL || ch_count == 0) return;

    for (i = 0; i < ch_count; i++) {
        if (i > 0) {
            ret = snprintf((char*)tx_buf + offset,
                           VOFA_TX_BUF_SIZE - (uint16_t)offset, ", ");
            if (ret > 0) offset += ret;
        }
        ret = snprintf((char*)tx_buf + offset,
                       VOFA_TX_BUF_SIZE - (uint16_t)offset, "%.4f", data[i]);
        if (ret > 0) offset += ret;
    }

    ret = snprintf((char*)tx_buf + offset,
                   VOFA_TX_BUF_SIZE - (uint16_t)offset, "\n");
    if (ret > 0) offset += ret;

    VOFA_Port_SendData(tx_buf, (uint16_t)offset);
}

void VOFA_SendRaw(const uint8_t* data, uint16_t len)
{
    if (!vofa_initialized || data == NULL || len == 0) return;
    VOFA_Port_SendData(data, len);
}
