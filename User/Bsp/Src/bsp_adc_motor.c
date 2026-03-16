/**
 * @file    bsp_adc_motor.c
 * @brief   ADC电机采样驱动实现
 * @details 实现ADC1/ADC2的配置、启动和数据读取
 *          支持DMA和轮询两种模式，自动故障切换
 * @see     bsp_adc_motor.h
 */

#include "bsp_adc_motor.h"

#include "adc.h"

#include <math.h>
#include <stddef.h>

/*============================================================================
 * 私有常量定义 - ADC转换参数
 *============================================================================*/

/** @brief ADC参考电压 (V) */
#define BSP_ADC_REF_VOLTAGE           (3.3f)
/** @brief ADC分辨率 (12位 = 4095) */
#define BSP_ADC_FULL_SCALE            (4095.0f)
/** @brief VBUS分压电阻比例 (25:1) */
#define BSP_ADC_VBUS_DIVIDER_RATIO    (25.0f)
/** @brief VBUS校准系数 (实际电压/测量电压 = 23.98/23.5) */
#define BSP_ADC_VBUS_CALIBRATION      (1.0204f)

/** @brief NTC热敏电阻参数 - 上拉电阻 (Ω) */
#define BSP_ADC_TEMP_PULLUP_OHM       (4700.0f)
/** @brief NTC热敏电阻参数 - 25°C时的阻值 (Ω) */
#define BSP_ADC_TEMP_R25_OHM          (10000.0f)
/** @brief NTC热敏电阻参数 - B值 */
#define BSP_ADC_TEMP_BETA             (3380.0f)
/** @brief NTC热敏电阻参数 - 标称温度 (K) */
#define BSP_ADC_TEMP_T0_KELVIN        (298.15f)

/*============================================================================
 * 错误码定义
 *============================================================================*/

#define BSP_ADC_ERROR_ADC1_CAL        (1U)   /**< @brief ADC1校准失败 */
#define BSP_ADC_ERROR_ADC2_CAL        (2U)   /**< @brief ADC2校准失败 */
#define BSP_ADC_ERROR_ADC1_DMA_START  (3U)   /**< @brief ADC1 DMA启动失败 */
#define BSP_ADC_ERROR_ADC2_INJ_START  (4U)   /**< @brief ADC2注入启动失败 */
#define BSP_ADC_ERROR_ADC1_START      (7U)   /**< @brief ADC1轮询启动失败 */
#define BSP_ADC_ERROR_ADC1_POLL_1    (8U)   /**< @brief ADC1轮询转换失败1 */
#define BSP_ADC_ERROR_ADC1_POLL_2    (9U)   /**< @brief ADC1轮询转换失败2 */
#define BSP_ADC_ERROR_ADC1_STOP      (10U)  /**< @brief ADC1停止失败 */

/*============================================================================
 * 私有变量定义
 *============================================================================*/

/** @brief ADC1 DMA缓冲区 [0]=VBUS, [1]=温度 */
static uint16_t s_adc1_dma_buf[2] = {0U, 0U};
/** @brief U相电流原始值 */
static volatile uint16_t s_phase_u_raw = 0U;
/** @brief V相电流原始值 */
static volatile uint16_t s_phase_v_raw = 0U;
/** @brief W相电流原始值 */
static volatile uint16_t s_phase_w_raw = 0U;
/** @brief 注入通道更新计数 */
static volatile uint32_t s_injected_update_count = 0U;
/** @brief ADC1工作模式 */
static uint8_t s_adc1_mode = BSP_ADC1_MODE_DMA;

/*============================================================================
 * 私有函数实现
 *============================================================================*/

/**
 * @brief ADC1常规通道轮询转换
 * @return uint8_t 0:成功, 其他:错误码
 * @note 手动启动ADC并轮询转换结果，用于DMA失败时的备份
 */
static uint8_t bsp_adc_regular_refresh(void)
{
    if (HAL_ADC_Start(&hadc1) != HAL_OK) {
        return BSP_ADC_ERROR_ADC1_START;
    }

    /* 读取VBUS通道 */
    if (HAL_ADC_PollForConversion(&hadc1, 10U) != HAL_OK) {
        (void)HAL_ADC_Stop(&hadc1);
        return BSP_ADC_ERROR_ADC1_POLL_1;
    }
    s_adc1_dma_buf[0] = (uint16_t)HAL_ADC_GetValue(&hadc1);

    /* 读取温度通道 */
    if (HAL_ADC_PollForConversion(&hadc1, 10U) != HAL_OK) {
        (void)HAL_ADC_Stop(&hadc1);
        return BSP_ADC_ERROR_ADC1_POLL_2;
    }
    s_adc1_dma_buf[1] = (uint16_t)HAL_ADC_GetValue(&hadc1);

    if (HAL_ADC_Stop(&hadc1) != HAL_OK) {
        return BSP_ADC_ERROR_ADC1_STOP;
    }

    return 0U;
}

/*============================================================================
 * 公共函数实现
 *============================================================================*/

/**
 * @brief 获取ADC1当前工作模式
 * @return uint8_t 模式标识
 */
uint8_t bsp_adc_motor_get_adc1_mode(void)
{
    return s_adc1_mode;
}

/**
 * @brief 切换到轮询备份模式
 * @return uint8_t 0:成功, 其他:错误码
 * @note 当DMA传输失败时自动调用此函数切换到轮询模式
 */
uint8_t bsp_adc_motor_use_polling_backup(void)
{
    uint8_t status;

    s_adc1_dma_buf[0] = 0U;
    s_adc1_dma_buf[1] = 0U;

    status = bsp_adc_regular_refresh();
    if (status == 0U) {
        s_adc1_mode = BSP_ADC1_MODE_POLLING;
    }

    return status;
}

/**
 * @brief 初始化ADC电机采样
 * @return uint8_t 0:成功, 其他:错误码
 * @note 初始化流程:
 *       1. 清空缓冲区和变量
 *       2. 校准ADC1和ADC2
 *       3. 启动ADC1 DMA传输（VBUS和温度）
 *       4. 启动ADC2注入中断（相电流）
 *       5. 如果DMA失败则自动切换到轮询模式
 */
uint8_t bsp_adc_motor_init(void)
{
    s_adc1_dma_buf[0] = 0U;
    s_adc1_dma_buf[1] = 0U;
    s_phase_u_raw = 0U;
    s_phase_v_raw = 0U;
    s_phase_w_raw = 0U;
    s_injected_update_count = 0U;
    s_adc1_mode = BSP_ADC1_MODE_DMA;

    /* 校准ADC1 */
    if (HAL_ADCEx_Calibration_Start(&hadc1, ADC_SINGLE_ENDED) != HAL_OK) {
        return BSP_ADC_ERROR_ADC1_CAL;
    }

    /* 校准ADC2 */
    if (HAL_ADCEx_Calibration_Start(&hadc2, ADC_SINGLE_ENDED) != HAL_OK) {
        return BSP_ADC_ERROR_ADC2_CAL;
    }

    /* 启动ADC1 DMA */
    if (HAL_ADC_Start_DMA(&hadc1, (uint32_t *)s_adc1_dma_buf, 2U) != HAL_OK) {
        uint8_t backup_status = bsp_adc_motor_use_polling_backup();
        if (backup_status != 0U) {
            return BSP_ADC_ERROR_ADC1_DMA_START;
        }
    }

    /* 启动ADC2注入中断 */
    if (HAL_ADCEx_InjectedStart_IT(&hadc2) != HAL_OK) {
        return BSP_ADC_ERROR_ADC2_INJ_START;
    }

    return 0U;
}

/**
 * @brief 获取母线电压原始ADC值
 * @return uint16_t ADC值
 */
uint16_t bsp_adc_get_vbus_raw(void)
{
    return s_adc1_dma_buf[0];
}

/**
 * @brief 获取温度原始ADC值
 * @return uint16_t ADC值
 */
uint16_t bsp_adc_get_temp_raw(void)
{
    return s_adc1_dma_buf[1];
}

/**
 * @brief 获取母线电压（伏特）
 * @return float 电压值
 * @note 公式: V = (ADC/4095) × 3.3 × 25 × 校准系数
 *       校准系数基于万用表实测值：23.98V / 23.5V = 1.0204
 */
float bsp_adc_get_vbus(void)
{
    return ((float)bsp_adc_get_vbus_raw() / BSP_ADC_FULL_SCALE) * BSP_ADC_REF_VOLTAGE * BSP_ADC_VBUS_DIVIDER_RATIO * BSP_ADC_VBUS_CALIBRATION;
}

/**
 * @brief 获取NTC温度（摄氏度）
 * @return float 温度值
 * @note 使用Steinhart-Hart方程计算:
 *       R_NTC = R_pullup × (Vcc - V_adc) / V_adc
 *       T = 1 / (1/T0 + (1/B)×ln(R/R25)) - 273.15
 */
float bsp_adc_get_temp(void)
{
    float raw = (float)bsp_adc_get_temp_raw();
    float v_adc;
    float r_ntc;

    /* 边界检查，防止除零 */
    if (raw <= 0.0f) {
        raw = 1.0f;
    }

    if (raw >= BSP_ADC_FULL_SCALE) {
        raw = BSP_ADC_FULL_SCALE - 1.0f;
    }

    /* 计算NTC阻值 */
    v_adc = (raw / BSP_ADC_FULL_SCALE) * BSP_ADC_REF_VOLTAGE;
    r_ntc = BSP_ADC_TEMP_PULLUP_OHM * (BSP_ADC_REF_VOLTAGE - v_adc) / v_adc;

    /* 计算温度 */
    return (1.0f / ((1.0f / BSP_ADC_TEMP_T0_KELVIN) + (logf(r_ntc / BSP_ADC_TEMP_R25_OHM) / BSP_ADC_TEMP_BETA))) - 273.15f;
}

/**
 * @brief 获取U相电流原始值
 * @return uint16_t ADC值
 */
uint16_t bsp_adc_get_phase_u_raw(void)
{
    return s_phase_u_raw;
}

/**
 * @brief 获取V相电流原始值
 * @return uint16_t ADC值
 */
uint16_t bsp_adc_get_phase_v_raw(void)
{
    return s_phase_v_raw;
}

/**
 * @brief 获取W相电流原始值
 * @return uint16_t ADC值
 */
uint16_t bsp_adc_get_phase_w_raw(void)
{
    return s_phase_w_raw;
}

/**
 * @brief 获取三相电流值
 * @param iu U相电流指针
 * @param iv V相电流指针
 * @param iw W相电流指针
 * @note 参数可以为NULL以跳过
 */
void bsp_adc_get_phase_current(int16_t *iu, int16_t *iv, int16_t *iw)
{
    if (iu != NULL) {
        *iu = (int16_t)s_phase_u_raw;
    }

    if (iv != NULL) {
        *iv = (int16_t)s_phase_v_raw;
    }

    if (iw != NULL) {
        *iw = (int16_t)s_phase_w_raw;
    }
}

/**
 * @brief 获取注入通道更新计数
 * @return uint32_t 计数
 * @note 用于检测ADC2是否正常工作
 */
uint32_t bsp_adc_get_injected_update_count(void)
{
    return s_injected_update_count;
}

/**
 * @brief ADC2注入转换完成回调
 * @note 在ADC2注入通道转换完成时自动调用，更新三相电流值
 */
void HAL_ADCEx_InjectedConvCpltCallback(ADC_HandleTypeDef *hadc)
{
    if (hadc->Instance == ADC2) {
        /* 读取三相电流ADC值 */
        s_phase_u_raw = (uint16_t)HAL_ADCEx_InjectedGetValue(hadc, ADC_INJECTED_RANK_1);
        s_phase_v_raw = (uint16_t)HAL_ADCEx_InjectedGetValue(hadc, ADC_INJECTED_RANK_2);
        s_phase_w_raw = (uint16_t)HAL_ADCEx_InjectedGetValue(hadc, ADC_INJECTED_RANK_3);
        s_injected_update_count++;
    }
}
