/**
 * @file    bsp_ctrl_sd.h
 * @brief   半桥驱动芯片使能控制
 * @details 本模块控制电机驱动芯片的使能引脚(SD, Shut Down)
 *          通过GPIO控制驱动芯片的工作状态，实现硬件级别的安全保护
 *
 * @note    工作原理:
 *          - SD引脚拉高: 驱动芯片正常工作
 *          - SD引脚拉低: 驱动芯片立即关闭所有输出（紧急停止）
 *
 * @note    典型应用:
 *          - 初始化时禁用驱动
 *          - 启动电机前使能驱动
 *          - 故障时禁用驱动
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
