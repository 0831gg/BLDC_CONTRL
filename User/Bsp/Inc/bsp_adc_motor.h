/**
 * @file    bsp_adc_motor.h
 * @brief   ADC1/ADC2 电机采样驱动
 * @details 本模块负责采集电机运行过程中的各种模拟信号:
 *          - 母线电压 (VBUS)
 *          - 电机温度 (NTC热敏电阻)
 *          - 三相电流 (IU, IV, IW) - 通过ADC2注入通道
 *
 * @note    ADC1配置 (DMA模式):
 *          - 通道1: VBUS采样 (ADC_INJ_RANK_1)
 *          - 通道2: 温度采样 (ADC_INJ_RANK_2)
 *
 * @note    ADC2配置 (注入中断模式):
 *          - 通道1: U相电流 (ADC_INJECTED_RANK_1)
 *          - 通道2: V相电流 (ADC_INJECTED_RANK_2)
 *          - 通道3: W相电流 (ADC_INJECTED_RANK_3)
 *
 * @note    电流采样原理:
 *          - 使用采样电阻检测相电流
 *          - 通过运放放大后送入ADC
 *          - 需要校准以消除偏置误差
 */

#ifndef __BSP_ADC_MOTOR_H
#define __BSP_ADC_MOTOR_H

#ifdef __cplusplus
extern "C" {
#endif

#include "main.h"

/** @brief ADC1工作模式: DMA传输 */
#define BSP_ADC1_MODE_DMA       (0U)
/** @brief ADC1工作模式: 轮询备份（DMA失败时使用） */
#define BSP_ADC1_MODE_POLLING   (1U)

/**
 * @brief 初始化ADC电机采样
 * @return uint8_t 0:成功, 其他:错误码
 * @note 执行ADC校准，启动DMA和注入中断
 */
uint8_t bsp_adc_motor_init(void);

/**
 * @brief 获取ADC1当前工作模式
 * @return uint8_t BSP_ADC1_MODE_DMA 或 BSP_ADC1_MODE_POLLING
 */
uint8_t bsp_adc_motor_get_adc1_mode(void);

/**
 * @brief 切换到轮询备份模式
 * @return uint8_t 0:切换成功, 其他:错误码
 * @note 当DMA模式失败时自动调用，也可手动切换
 */
uint8_t bsp_adc_motor_use_polling_backup(void);

/* VBUS/温度 - 常规通道采样 */

/**
 * @brief 获取母线电压原始ADC值
 * @return uint16_t ADC原始值 (0-4095)
 */
uint16_t bsp_adc_get_vbus_raw(void);

/**
 * @brief 获取温度传感器原始ADC值
 * @return uint16_t ADC原始值 (0-4095)
 */
uint16_t bsp_adc_get_temp_raw(void);

/**
 * @brief 获取母线电压（伏特）
 * @return float 电压值 (V)
 * @note 使用分压电阻比例计算: VBUS = ADC×3.3×25/4095
 */
float bsp_adc_get_vbus(void);

/**
 * @brief 获取电机温度（摄氏度）
 * @return float 温度值 (°C)
 * @note 使用NTC热敏电阻公式计算，需根据具体型号调整参数
 */
float bsp_adc_get_temp(void);

/* 相电流 - 注入通道采样 */

/**
 * @brief获取U相电流原始ADC值
 * @return uint16_t ADC原始值
 */
uint16_t bsp_adc_get_phase_u_raw(void);

/**
 * @brief 获取V相电流原始ADC值
 * @return uint16_t ADC原始值
 */
uint16_t bsp_adc_get_phase_v_raw(void);

/**
 * @brief 获取W相电流原始ADC值
 * @return uint16_t ADC原始值
 */
uint16_t bsp_adc_get_phase_w_raw(void);

/**
 * @brief 获取三相电流值
 * @param iu U相电流输出指针
 * @param iv V相电流输出指针
 * @param iw W相电流输出指针
 * @note 参数可以为NULL，函数会跳过对应的赋值
 */
void bsp_adc_get_phase_current(int16_t *iu, int16_t *iv, int16_t *iw);

/**
 * @brief 获取注入通道更新计数
 * @return uint32_t 注入转换完成次数
 * @note 可用于判断ADC是否正常工作
 */
uint32_t bsp_adc_get_injected_update_count(void);

#ifdef __cplusplus
}
#endif

#endif /* __BSP_ADC_MOTOR_H */
