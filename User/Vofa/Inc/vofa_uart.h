#ifndef __VOFA_UART_H
#define __VOFA_UART_H

#include "vofa_port.h"
#include <stdint.h>
#include <stdarg.h>

/*=============================================================================
 *  VOFA+ 协议层（与硬件完全无关）
 *
 *  支持协议:
 *    - JustFloat : 小端浮点 + 帧尾 0x00007F80，高效实时波形
 *    - FireWater : 字符串格式 "v1,v2,...\n"，可同时打印调试和波形
 *    - RawData   : 原始字节透传
 *=============================================================================*/

/* VOFA运行信息 */
typedef struct {
    uint32_t baudrate;      /* 实际使用的波特率 */
    uint8_t  over8;         /* 过采样模式 1=OVER8 0=OVER16 */
    uint8_t  baud_index;    /* 波特率在降级表中的索引 */
    uint8_t  initialized;   /* 初始化完成标志 */
    float    baud_error;    /* 实际波特率误差(%) */
} VOFA_Info_t;

/*========================== 初始化 ==========================*/

/**
 * @brief  初始化VOFA+串口（自动选择最高可用波特率）
 * @note   内部执行: 波特率扫描 → 选择最佳 → GPIO初始化 → USART初始化
 */
void VOFA_Init(void);

/**
 * @brief  以指定波特率初始化（跳过自动选择）
 * @param  baudrate: 强制使用的波特率
 * @param  over8:    过采样模式
 */
void VOFA_InitWithBaud(uint32_t baudrate, uint8_t over8);

/*========================== 查询 ==========================*/

/**
 * @brief  获取当前使用的波特率
 */
uint32_t VOFA_GetBaudrate(void);

/**
 * @brief  获取完整运行信息
 */
const VOFA_Info_t* VOFA_GetInfo(void);

/*======================== 数据发送 ========================*/

/**
 * @brief  JustFloat协议 - 发送多通道浮点数据
 * @param  data:     浮点数组
 * @param  ch_count: 通道数
 * @note   VOFA+上位机选择 "JustFloat" 协议
 *         格式: [float0][float1]...[floatN][0x00 0x00 0x80 0x7F]
 */
void VOFA_JustFloat(float* data, uint8_t ch_count);

/**
 * @brief  FireWater协议 - 格式化打印
 * @note   VOFA+上位机选择 "FireWater" 协议
 *         支持printf风格格式化
 */
void VOFA_Printf(const char* fmt, ...);

/**
 * @brief  FireWater协议 - 发送多通道浮点波形
 * @param  data:     浮点数组
 * @param  ch_count: 通道数
 * @note   自动生成 "val1,val2,...,valN\n" 格式
 */
void VOFA_FireWaterFloat(float* data, uint8_t ch_count);

/**
 * @brief  发送原始字节
 */
void VOFA_SendRaw(const uint8_t* data, uint16_t len);

/*========================== 测试 ==========================*/

#if VOFA_ENABLE_TEST
/**
 * @brief  运行指定测试
 * @param  test_id: 测试编号 1~5
 */
void VOFA_RunTest(uint8_t test_id);
#endif

#endif /* __VOFA_UART_H */
