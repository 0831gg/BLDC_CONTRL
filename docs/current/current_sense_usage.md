# 三相电流采样使用说明

## 概述

本文档说明如何使用新增的电流采样模块 `bsp_current_sense` 来实现三相电流的采集、转换和控制。

## 硬件配置

### ADC配置（已完成，无需修改CubeMX）

- **ADC1**: 母线电压(VBUS) + 温度(TEMP) - DMA循环采样
- **ADC2**: 三相电流(U/V/W) - 注入通道，TIM1硬件触发
- **触发源**: TIM1_CH4 (OC4REF) - PWM中心点触发

### 引脚定义

| 信号 | 引脚 | ADC通道 | 说明 |
|------|------|---------|------|
| VBUS | PA3 | ADC1_IN4 | 母线电压（25:1分压） |
| TEMP | PC0 | ADC1_IN6 | NTC温度传感器 |
| AMP_U | PC1 | ADC2_IN7 | U相电流（注入通道1） |
| AMP_V | PC2 | ADC2_IN8 | V相电流（注入通道2） |
| AMP_W | PC3 | ADC2_IN9 | W相电流（注入通道3） |

## 电流采样原理

### 硬件电路

```
电机相电流 → 采样电阻(0.02Ω) → 运放放大(6倍) → ADC输入
```

### 转换公式

```c
// ADC原始值 → 电流值(mA)
I(mA) = (ADC_raw - ADC_offset) × 6.71

// 其中:
// ADC_offset = 2048 (零电流时的ADC值，通过校准获得)
// 6.71 = 3.3V / (4095 × 6 × 0.02Ω) × 1000
```

### 采样时机

- ADC2注入通道由TIM1_CH4触发
- 触发时刻：PWM周期的中心点
- 优点：避开开关噪声，获得准确的电流值

## 软件使用

### 1. 初始化

在 `Test_Init()` 中已自动完成：

```c
// 初始化ADC
bsp_adc_motor_init();

// 启动ADC触发
__HAL_TIM_SET_COMPARE(&htim1, TIM_CHANNEL_4, BSP_PWM_PERIOD / 2U);
HAL_TIM_PWM_Start(&htim1, TIM_CHANNEL_4);

// 初始化电流采样（包含零点校准）
bsp_current_sense_init();
```

### 2. 零点校准

**重要**: 必须在电机停止、无电流时执行校准！

```c
// 自动校准（采样200次求平均）
bsp_current_sense_calibrate(200);

// 获取校准结果
bsp_current_calib_t calib;
bsp_current_sense_get_calib(&calib);
printf("U_offset=%u V_offset=%u W_offset=%u\n", 
       calib.u_offset, calib.v_offset, calib.w_offset);

// 手动设置校准值（可选）
calib.u_offset = 2048;
calib.v_offset = 2050;
calib.w_offset = 2045;
bsp_current_sense_set_calib(&calib);
```

### 3. 读取电流值

#### 方法A: 读取原始值（无滤波）

```c
bsp_current_t current;
bsp_current_sense_get_raw(&current);

printf("Iu=%.1fmA Iv=%.1fmA Iw=%.1fmA\n", 
       current.u_ma, current.v_ma, current.w_ma);
```

#### 方法B: 读取滤波值（推荐）

```c
// 在主循环中周期性更新滤波器
bsp_current_sense_update_filter();

// 读取滤波后的电流
bsp_current_t current;
bsp_current_sense_get_filtered(&current);

printf("Iu=%.1fmA Iv=%.1fmA Iw=%.1fmA\n", 
       current.u_ma, current.v_ma, current.w_ma);
```

### 4. 过流保护

```c
// 检测过流（阈值5000mA）
if (bsp_current_sense_is_overcurrent()) {
    motor_stop();
    printf("OVERCURRENT!\n");
}

// 获取最大相电流
float max_current = bsp_current_sense_get_max_phase_current();
printf("Max phase current: %.1fmA\n", max_current);
```

## 配置参数

在 `bsp_current_sense.h` 中可修改以下参数：

```c
// 采样电阻阻值 (Ω)
#define BSP_CURRENT_SENSE_SHUNT_OHM         (0.02f)

// 运放增益 (V/V)
#define BSP_CURRENT_SENSE_AMP_GAIN          (6.0f)

// 过流保护阈值 (mA)
#define BSP_CURRENT_SENSE_OC_THRESHOLD_MA   (5000.0f)

// 低通滤波器系数 (0-1, 越大响应越快)
#define BSP_CURRENT_SENSE_LPF_ALPHA         (0.1f)
```

## 调试技巧

### 1. 验证ADC是否工作

```c
// 检查注入通道更新计数
uint32_t count = bsp_adc_get_injected_update_count();
printf("ADC2 update count: %lu\n", count);
// 如果计数不增加，说明ADC2未正常工作
```

### 2. 检查零点偏移

```c
// 在电机停止时读取原始ADC值
uint16_t u_raw = bsp_adc_get_phase_u_raw();
uint16_t v_raw = bsp_adc_get_phase_v_raw();
uint16_t w_raw = bsp_adc_get_phase_w_raw();
printf("Raw ADC: U=%u V=%u W=%u\n", u_raw, v_raw, w_raw);
// 理想值应接近2048
```

### 3. 观察电流波形

使用Vofa+或串口助手实时显示电流波形：

```c
// 在主循环中发送
bsp_current_t current;
bsp_current_sense_get_filtered(&current);
printf("%.2f,%.2f,%.2f\n", current.u_ma, current.v_ma, current.w_ma);
```

## 常见问题

### Q1: 电流值始终为0

**原因**: 
- ADC2未启动
- TIM1_CH4未启动（无触发信号）
- 校准偏移值错误

**解决**:
```c
// 检查ADC2是否启动
if (HAL_ADCEx_InjectedStart_IT(&hadc2) != HAL_OK) {
    printf("ADC2 start failed!\n");
}

// 检查TIM1_CH4是否启动
if (HAL_TIM_PWM_Start(&htim1, TIM_CHANNEL_4) != HAL_OK) {
    printf("TIM1_CH4 start failed!\n");
}
```

### Q2: 电流值异常大或异常小

**原因**: 
- 校准偏移值不准确
- 硬件增益与软件配置不匹配

**解决**:
```c
// 重新校准
bsp_current_sense_calibrate(500);  // 增加采样次数

// 检查硬件增益，修改配置
#define BSP_CURRENT_SENSE_AMP_GAIN  (实际增益)
```

### Q3: 电流波动很大

**原因**: 
- 滤波系数太大
- 采样时机不对（开关噪声）

**解决**:
```c
// 减小滤波系数
#define BSP_CURRENT_SENSE_LPF_ALPHA  (0.05f)  // 原来0.1

// 确认TIM1_CH4比较值设置在中心点
__HAL_TIM_SET_COMPARE(&htim1, TIM_CHANNEL_4, BSP_PWM_PERIOD / 2U);
```

## 示例代码

完整示例见 `User/Test/Src/test.c` 中的实现。

主要流程：

1. **初始化**: `Test_Init()` - 自动完成ADC和电流采样初始化
2. **更新滤波**: `Test_Loop()` - 周期性调用 `bsp_current_sense_update_filter()`
3. **读取显示**: `print_status()` - 打印电流值和过流状态
4. **过流保护**: `Test_Loop()` - 检测过流并停机

## 性能指标

- **采样频率**: 10kHz (与PWM频率同步)
- **分辨率**: 6.71 mA/LSB
- **测量范围**: ±13.7A (理论最大)
- **过流阈值**: 5A (可配置)
- **滤波延迟**: ~10ms (α=0.1时)

## 总结

✅ **无需修改CubeMX配置** - 现有配置已完善  
✅ **硬件触发同步采样** - 精确度高，抗干扰强  
✅ **自动零点校准** - 消除偏置误差  
✅ **一阶低通滤波** - 平滑电流波形  
✅ **过流保护** - 自动检测并停机  

现在你可以直接编译运行，查看电流采样效果！
