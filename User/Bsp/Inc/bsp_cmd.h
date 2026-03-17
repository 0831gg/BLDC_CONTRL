/**
 * @file    bsp_cmd.h
 * @brief   轻量ASCII串口命令解析
 */

#ifndef __BSP_CMD_H
#define __BSP_CMD_H

#ifdef __cplusplus
extern "C" {
#endif

#include "main.h"

void bsp_cmd_init(void);
void bsp_cmd_process(void);
void bsp_cmd_rx_callback(uint8_t byte);
uint8_t *bsp_cmd_get_rx_byte_ptr(void);

#ifdef __cplusplus
}
#endif

#endif /* __BSP_CMD_H */
