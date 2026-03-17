/**
 * @file    bsp_current_sense.h
 * @brief   三相电流采样与转换模块
 * @details 提供三相电流的ADC原始值到实际电流值的转换
 *          支持零点校准、滤波和过流保护
 */

#ifndef __BSP_CURRENT_SENSE_H
#define __BSP_CURRENT_SENSE_H

#ifdef __cplusplus
extern "C" {
#endif

#include "main.h"
#include <stdbool.h>

/*============================================================================
 * 电流采样参数配置
 *============================================================================*/

/** @brief 采样电阻阻值 (Ω) */
#define BSP_CURRENT_SENSE_SHUNT_OHM         (0.02f)

/** @brief 运放增益 (V/V) */
#define BSP_CURRENT_SENSE_AMP_GAIN          (6.0f)

/** @brief ADC参考电压 (V) */
#define BSP_CURRENT_SENSE_VREF              (3.3f)

/** @brief ADC分辨率 (12位) */
#define BSP_CURRENT_SENSE_ADC_MAX           (4095.0f)

/** @brief ADC中点值（零电流对应的ADC值） */
#define BSP_CURRENT_SENSE_ADC_ZERO          (2048U)

/** @brief 电流转换系数 (A/LSB)
 *  @note 公式: I = (ADC - 2048) × Vref / (ADC_MAX × Gain × R_shunt)
 *        I = (ADC - 2048) × 3.3 / (4095 × 6 × 0.02)
 *        I = (ADC - 2048) × 0.00671 A
 *        转换为mA: × 1000 = 6.71 mA/LSB
 */
#define BSP_CURRENT_SENSE_LSB_TO_MA         (6.71f)

/** @brief 过流保护阈值 (mA) */
#define BSP_CURRENT_SENSE_OC_THRESHOLD_MA   (5000.0f)

/** @brief 一阶低通滤波器系数 (0-1, 越大响应越快) */
#define BSP_CURRENT_SENSE_LPF_ALPHA         (0.1f)

/*============================================================================
 * 数据结构定义
 *============================================================================*/

/**
 * @brief 三相电流数据结构
 */
typedef struct {
    float u_ma;         /**< U相电流 (mA) */
    float v_ma;         /**< V相电流 (mA) */
    float w_ma;         /**< W相电流 (mA) */
    float bus_ma;       /**< 母线电流 (mA) - 计算值 */
} bsp_current_t;

/**
 * @brief 电流校准数据结构
 */
typedef struct {
    uint16_t u_offset;  /**< U相零点偏移 */
    uint16_t v_offset;  /**< V相零点偏移 */
    uint16_t w_offset;  /**< W相零点偏移 */
    bool calibrated;    /**< 校准完成标志 */
} bsp_current_calib_t;

/*============================================================================
 * 公共函数声明
 *============================================================================*/

/**
 * @brief 初始化电流采样模块
 * @return uint8_t 0:成功, 其他:错误码
 * @note 执行零点校准，需要在电机停止时调用
 */
uint8_t bsp_current_sense_init(void);

/**
 * @brief 执行零点校准
 * @param samples 采样次数（建议100-500）
 * @return uint8_t 0:成功, 其他:错误码
 * @note 必须在电机停止、无电流流过时执行
 */
uint8_t bsp_current_sense_calibrate(uint16_t samples);

/**
 * @brief 获取校准状态
 * @return bool true:已校准, false:未校准
 */
bool bsp_current_sense_is_calibrated(void);

/**
 * @brief 获取校准偏移值
 * @param calib 校准数据输出指针
 */
void bsp_current_sense_get_calib(bsp_current_calib_t *calib);

/**
 * @brief 手动设置校准偏移值
 * @param calib 校准数据指针
 */
void bsp_current_sense_set_calib(const bsp_current_calib_t *calib);

/**
 * @brief 获取三相电流（原始值，无滤波）
 * @param current 电流数据输出指针
 */
void bsp_current_sense_get_raw(bsp_current_t *current);

/**
 * @brief 获取三相电流（滤波后）
 * @param current 电流数据输出指针
 * @note 使用一阶低通滤波器平滑电流值
 */
void bsp_current_sense_get_filtered(bsp_current_t *current);

/**
 * @brief 更新滤波器（需要周期性调用）
 * @note 建议在主循环或定时器中调用，频率1-10kHz
 */
void bsp_current_sense_update_filter(void);

/**
 * @brief 检测过流状态
 * @return bool true:过流, false:正常
 */
bool bsp_current_sense_is_overcurrent(void);

/**
 * @brief 获取最大相电流（绝对值）
 * @return float 最大电流 (mA)
 */
float bsp_current_sense_get_max_phase_current(void);

/**
 * @brief ADC原始值转换为电流值
 * @param adc_raw ADC原始值
 * @param offset 零点偏移
 * @return float 电流值 (mA)
 */
float bsp_current_sense_adc_to_ma(uint16_t adc_raw, uint16_t offset);

#ifdef __cplusplus
}
#endif

#endif /* __BSP_CURRENT_SENSE_H */
