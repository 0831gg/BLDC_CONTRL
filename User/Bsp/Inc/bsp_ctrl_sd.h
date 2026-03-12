/**
 * @file    bsp_ctrl_sd.h
 * @brief   半桥驱动使能控制
 */

#ifndef __BSP_CTRL_SD_H
#define __BSP_CTRL_SD_H

#ifdef __cplusplus
extern "C" {
#endif

#include "main.h"

void bsp_ctrl_sd_init(void);
void bsp_ctrl_sd_enable(void);
void bsp_ctrl_sd_disable(void);
uint8_t bsp_ctrl_sd_is_enabled(void);

#ifdef __cplusplus
}
#endif

#endif /* __BSP_CTRL_SD_H */
