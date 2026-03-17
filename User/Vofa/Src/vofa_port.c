#include "vofa_port.h"

/* Send error counter kept for VOFA-specific diagnostics. */
static volatile uint32_t s_tx_error_count = 0U;

void VOFA_Port_SendData(const uint8_t* data, uint16_t len)
{
    HAL_StatusTypeDef status;

    if (data == NULL || len == 0U) {
        return;
    }

    if (len > VOFA_TX_BUF_SIZE) {
        len = VOFA_TX_BUF_SIZE;
    }

    status = BSP_UART_Transmit(data, len, BSP_UART_TX_TIMEOUT_MS);
    if (status != HAL_OK) {
        s_tx_error_count++;
    }
}

uint8_t VOFA_Port_IsBusy(void)
{
    return BSP_UART_IsBusy();
}

uint32_t VOFA_Port_GetErrorCount(void)
{
    return s_tx_error_count;
}

void VOFA_Port_ClearErrorCount(void)
{
    s_tx_error_count = 0U;
}
