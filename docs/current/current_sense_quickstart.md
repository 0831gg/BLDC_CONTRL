# 电流采样功能 - 快速开始

## 🚀 立即使用

### 1. 编译工程

打开Keil工程并编译：

```
文件位置: D:\Electrical Design\MOTOR\My_Motor_Control_Project\my_control\MDK-ARM\BLDC_HALL.uvprojx

操作步骤:
1. 双击打开 BLDC_HALL.uvprojx
2. 按 F7 编译
3. 按 F8 下载到开发板
```

### 2. 查看串口输出

连接串口（115200波特率），你将看到：

```
[P5] BLDC Motor Control Test
[P5] Calibrating current sensors...
[P5] Current calibration: U=2048 V=2050 W=2045
[P5] Ready!

[P5] Run=0 Dir=CW Hall=101 Duty=0% RPM=0
     VBUS=24.000V Temp=25.5C
     Iu=0.0mA Iv=0.0mA Iw=0.0mA Imax=0.0mA
```

### 3. 测试电流采样

**按键操作**:
- **KEY0**: 启动/停止电机
- **KEY1**: 切换方向
- **KEY2**: 调整占空比

**启动电机后**，串口将显示实时电流：

```
[P5] Run=1 Dir=CW Hall=101 Duty=20% RPM=1250
     VBUS=23.850V Temp=26.2C
     Iu=850.5mA Iv=-425.2mA Iw=-425.3mA Imax=850.5mA
```

### 4. 测试过流保护

增大占空比至电流超过5A，系统将自动停机：

```
[P5] OVERCURRENT PROTECTION!
[P5] Motor stopped
```

## ✅ 已完成的功能

- ✅ 三相电流实时采样（10kHz）
- ✅ 自动零点校准
- ✅ 一阶低通滤波
- ✅ 过流保护（5A阈值）
- ✅ 串口实时显示

## 📝 无需配置

**CubeMX配置已完善，无需修改！**

现有配置：
- ADC1: VBUS + TEMP (DMA循环)
- ADC2: U/V/W电流 (注入通道，TIM1触发)
- TIM1_CH4: ADC触发源（PWM中心点）

## 🔧 可调参数

如需修改，编辑 `User/Bsp/Inc/bsp_current_sense.h`:

```c
// 过流保护阈值 (mA)
#define BSP_CURRENT_SENSE_OC_THRESHOLD_MA   (5000.0f)

// 滤波系数 (0-1, 越大响应越快)
#define BSP_CURRENT_SENSE_LPF_ALPHA         (0.1f)

// 采样电阻阻值 (Ω) - 根据硬件修改
#define BSP_CURRENT_SENSE_SHUNT_OHM         (0.02f)

// 运放增益 (V/V) - 根据硬件修改
#define BSP_CURRENT_SENSE_AMP_GAIN          (6.0f)
```

## 📚 详细文档

- **使用说明**: `docs/current_sense_usage.md`
- **实现总结**: `docs/current_sense_implementation.md`

## ❓ 常见问题

### Q: 电流值始终为0？

检查ADC2和TIM1_CH4是否启动（代码已自动处理）

### Q: 电流值不准确？

在电机停止时重新校准：
```c
bsp_current_sense_calibrate(500);
```

### Q: 如何修改过流阈值？

修改 `BSP_CURRENT_SENSE_OC_THRESHOLD_MA` 宏定义

## 🎯 下一步

基于电流采样，你可以实现：

1. **电流环控制** - PI控制器
2. **转矩控制** - 基于电流的转矩估算
3. **功率监测** - P = U × I
4. **效率分析** - η = Pout / Pin
5. **故障诊断** - 缺相、不平衡检测

---

**现在就开始测试吧！** 🚀
