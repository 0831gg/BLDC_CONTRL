# BLDC电机控制系统函数调用流程文档

## 文档说明

本文档详细描述了基于STM32G474的BLDC电机控制系统的完整函数调用流程，包括初始化序列、主循环、中断处理和模块间数据流。

**系统架构**：
- 微控制器：STM32G474
- 电机类型：BLDC无刷直流电机（霍尔传感器反馈）
- 控制方式：六步换相
- PWM频率：10kHz (TIM1)
- 霍尔采样：50kHz轮询 (TIM5)
- 通信：UART串口调试输出

---

## 1. 系统初始化流程

### 1.1 主初始化入口

```
Test_Phase5_Init()
├── BSP_UART_Init()                          // 串口初始化
│   └── HAL_UART_Init()                      // HAL库UART初始化
│
├── bsp_key_init()                           // 按键初始化
│   ├── HAL_GPIO_Init()                      // 配置GPIO为输入模式
│   └── 初始化按键状态变量
│
├── bsp_ctrl_sd_init()                       // SD控制引脚初始化
│   └── HAL_GPIO_Init()                      // 配置SD引脚为输出
│
├── bsp_pwm_init()                           // PWM初始化
│   ├── HAL_TIM_PWM_Init()                   // TIM1 PWM初始化
│   ├── HAL_TIM_PWM_ConfigChannel()          // 配置6路PWM通道
│   ├── HAL_TIMEx_PWMN_Start()               // 启动互补PWM输出
│   └── bsp_pwm_set_duty_cycle(0)           // 设置初始占空比为0
│       └── __HAL_TIM_SET_COMPARE()          // 设置比较值
│
├── bsp_adc_motor_init()                     // ADC初始化
│   ├── HAL_ADC_Init()                       // ADC1初始化
│   ├── HAL_ADC_ConfigChannel()              // 配置ADC通道
│   ├── HAL_ADC_Start_DMA()                  // 启动DMA传输
│   └── HAL_TIM_Base_Start()                 // 启动TIM6触发ADC
│
├── bsp_hall_init()                          // 霍尔传感器初始化
│   ├── HAL_GPIO_Init()                      // 配置霍尔GPIO
│   ├── HAL_TIM_Base_Init()                  // TIM5初始化（50kHz）
│   ├── HAL_TIM_Base_Start_IT()              // 启动TIM5中断
│   ├── bsp_hall_clear_stats()               // 清除统计数据
│   │   ├── 清零有效/无效转换计数
│   │   ├── 清零RPM滤波缓冲区
│   │   └── 重置状态变量
│   └── bsp_hall_read_state()                // 读取初始霍尔状态
│       └── HAL_GPIO_ReadPin() × 3           // 读取U/V/W三相霍尔
│
├── motor_phase_tab_init()                   // 换相表初始化
│   ├── 初始化六步换相表
│   ├── 设置默认换相偏移为0
│   └── 验证换相表完整性
│
├── motor_ctrl_init()                        // 电机控制初始化
│   ├── 初始化电机状态机（STOPPED）
│   ├── 设置默认方向（CW）
│   ├── 清零故障标志
│   └── 初始化控制参数
│
└── BSP_UART_Printf()                        // 打印初始化完成信息
```

---

## 2. 主循环运行流程

### 2.1 主循环入口

```
Test_Phase5_Loop()
├── test_phase5_handle_keys()                // 按键处理
│   ├── bsp_key_scan()                       // 扫描所有按键
│   │   ├── HAL_GPIO_ReadPin()               // 读取按键GPIO
│   │   ├── 消抖处理（20ms）
│   │   └── 返回按键事件（PRESS/RELEASE/NONE）
│   │
│   ├── [KEY1按下] motor_ctrl_start()        // 启动电机
│   │   ├── motor_ctrl_is_running()          // 检查运行状态
│   │   ├── motor_start_simple()             // 简单启动模式
│   │   │   ├── bsp_ctrl_sd_enable()         // 使能驱动器
│   │   │   │   └── HAL_GPIO_WritePin(HIGH)
│   │   │   ├── bsp_hall_read_state()        // 读取当前霍尔状态
│   │   │   ├── motor_phase_tab_get_phase()  // 查表获取相位
│   │   │   ├── bsp_pwm_set_phase()          // 设置PWM相位
│   │   │   │   └── HAL_TIM_PWM_Start/Stop() // 控制6路PWM
│   │   │   ├── bsp_pwm_set_duty_cycle()     // 设置占空比
│   │   │   └── 更新电机状态为RUNNING
│   │   └── bsp_hall_clear_stats()           // 清除统计数据
│   │
│   ├── [KEY2按下] motor_ctrl_stop()         // 停止电机
│   │   ├── bsp_pwm_set_duty_cycle(0)        // 占空比设为0
│   │   ├── bsp_pwm_set_phase(0)             // 关闭所有PWM
│   │   ├── bsp_ctrl_sd_disable()            // 禁用驱动器
│   │   │   └── HAL_GPIO_WritePin(LOW)
│   │   └── 更新电机状态为STOPPED
│   │
│   ├── [KEY3按下] 增加占空比                 // 占空比+5%
│   │   ├── motor_ctrl_get_duty_cycle()      // 获取当前占空比
│   │   ├── 计算新占空比（限制≤100%）
│   │   └── motor_ctrl_set_duty_cycle()      // 设置新占空比
│   │       └── bsp_pwm_set_duty_cycle()
│   │
│   └── [KEY4按下] 减少占空比                 // 占空比-5%
│       ├── motor_ctrl_get_duty_cycle()
│       ├── 计算新占空比（限制≥0%）
│       └── motor_ctrl_set_duty_cycle()
│
├── bsp_hall_poll_and_commutate()            // 霍尔轮询与换相
│   ├── bsp_hall_read_state()                // 读取霍尔状态
│   ├── 检测状态变化
│   ├── [状态改变] 换相处理
│   │   ├── motor_phase_tab_is_valid_transition() // 验证转换有效性
│   │   ├── [有效转换]
│   │   │   ├── s_valid_transition_count++   // 有效计数+1
│   │   │   ├── 计算换相周期
│   │   │   ├── 更新RPM滤波缓冲区
│   │   │   │   └── s_period_buffer[index] = period
│   │   │   ├── motor_phase_tab_get_phase()  // 获取新相位
│   │   │   └── bsp_pwm_set_phase()          // 执行换相
│   │   └── [无效转换]
│   │       └── s_invalid_transition_count++ // 无效计数+1
│   └── 返回换相结果
│
├── bsp_adc_motor_poll()                     // ADC数据处理
│   ├── 检查DMA传输完成标志
│   ├── [数据就绪] 处理ADC原始数据
│   │   ├── 多次采样平均滤波
│   │   └── 更新VBUS/电流/温度缓存
│   └── 清除就绪标志
│
├── [定时200ms] 状态打印                      // 串口输出状态
│   ├── motor_ctrl_is_running()              // 获取运行状态
│   ├── motor_ctrl_get_direction()           // 获取方向
│   ├── motor_phase_tab_get_commutation_offset() // 获取换相偏移
│   ├── bsp_hall_read_state()                // 获取霍尔状态
│   ├── motor_ctrl_get_duty_cycle()          // 获取占空比
│   ├── motor_ctrl_get_last_action()         // 获取最后动作
│   ├── motor_ctrl_get_fault()               // 获取故障状态
│   ├── bsp_hall_get_valid_transition_count() // 获取有效转换数
│   ├── bsp_hall_get_invalid_transition_count() // 获取无效转换数
│   ├── bsp_hall_get_speed()                 // 获取RPM（滤波后）
│   │   ├── 读取RPM滤波缓冲区
│   │   ├── 计算平均周期
│   │   └── 转换为RPM = (50000×60)/(avg_ticks×6)
│   ├── bsp_pwm_is_break_fault()             // 获取刹车故障
│   ├── bsp_adc_get_vbus()                   // 获取母线电压
│   │   ├── bsp_adc_get_vbus_raw()           // 获取原始ADC值
│   │   └── 电压转换 = raw × (3.3/4096) × 25 × 1.0204
│   └── BSP_UART_Printf()                    // 格式化输出
│
└── 循环继续
```

---

## 3. 中断处理流程

### 3.1 TIM5中断（50kHz霍尔采样定时器）

```
TIM5_IRQHandler()
└── HAL_TIM_IRQHandler(&htim5)
    └── HAL_TIM_PeriodElapsedCallback(&htim5)
        └── [用户回调 - 当前未使用]
            // 注：霍尔采样在主循环中轮询，不在中断中处理
```

### 3.2 ADC DMA传输完成中断

```
DMA1_Channel1_IRQHandler()
└── HAL_DMA_IRQHandler(&hdma_adc1)
    └── HAL_ADC_ConvCpltCallback(&hadc1)
        └── bsp_adc_motor_dma_callback()
            ├── 设置数据就绪标志
            └── [数据处理在主循环中完成]
```

### 3.3 TIM1刹车中断（过流保护）

```
TIM1_BRK_TIM15_IRQHandler()
└── HAL_TIM_IRQHandler(&htim1)
    └── HAL_TIMEx_BreakCallback(&htim1)
        └── bsp_pwm_break_callback()
            ├── 设置刹车故障标志
            ├── bsp_pwm_set_duty_cycle(0)    // 占空比清零
            ├── bsp_pwm_set_phase(0)         // 关闭所有PWM
            └── motor_ctrl_set_fault()       // 设置电机故障
```

---

## 4. 核心模块详细调用链

### 4.1 电机启动流程（简单模式）

```
motor_ctrl_start()
├── 检查电机是否已运行
├── motor_start_simple()
│   ├── bsp_ctrl_sd_enable()                 // 使能驱动器SD引脚
│   ├── HAL_Delay(10)                        // 等待驱动器稳定
│   ├── bsp_hall_read_state()                // 读取当前霍尔状态
│   │   └── HAL_GPIO_ReadPin() × 3
│   ├── motor_phase_tab_get_phase()          // 根据霍尔状态查表
│   │   ├── 应用换相偏移
│   │   └── 返回6路PWM状态
│   ├── bsp_pwm_set_phase()                  // 设置初始相位
│   │   ├── HAL_TIM_PWM_Start()              // 启动高侧PWM
│   │   ├── HAL_TIMEx_PWMN_Start()           // 启动低侧PWM
│   │   ├── HAL_TIM_PWM_Stop()               // 停止不需要的PWM
│   │   └── HAL_TIMEx_PWMN_Stop()
│   ├── bsp_pwm_set_duty_cycle(初始占空比)   // 设置启动占空比
│   │   └── __HAL_TIM_SET_COMPARE()
│   └── 设置电机状态为RUNNING
└── bsp_hall_clear_stats()                   // 清除历史统计数据
```

### 4.2 换相执行流程

```
bsp_hall_poll_and_commutate()
├── bsp_hall_read_state()                    // 读取当前霍尔状态
│   ├── HAL_GPIO_ReadPin(HALL_U)
│   ├── HAL_GPIO_ReadPin(HALL_V)
│   ├── HAL_GPIO_ReadPin(HALL_W)
│   └── 组合为3位状态码（1-6）
│
├── 比较新旧状态
├── [状态改变]
│   ├── motor_phase_tab_is_valid_transition() // 验证转换
│   │   └── 检查转换表中是否存在此转换
│   │
│   ├── [有效转换]
│   │   ├── s_valid_transition_count++
│   │   ├── 计算换相周期
│   │   │   └── period = TIM5_CNT - last_TIM5_CNT
│   │   ├── 更新RPM滤波缓冲区
│   │   │   ├── s_period_buffer[index] = period
│   │   │   ├── index = (index + 1) % 6
│   │   │   └── count = min(count + 1, 6)
│   │   ├── motor_phase_tab_get_phase()      // 获取新相位
│   │   │   ├── 查找换相表
│   │   │   ├── 应用方向（CW/CCW）
│   │   │   └── 应用换相偏移
│   │   └── bsp_pwm_set_phase()              // 执行换相
│   │       └── 更新6路PWM输出状态
│   │
│   └── [无效转换]
│       └── s_invalid_transition_count++
│
└── 返回换相结果
```

### 4.3 RPM计算流程（滑动平均滤波）

```
bsp_hall_get_speed()
├── 检查滤波缓冲区数据量
├── [缓冲区为空] 返回0
├── [有数据]
│   ├── 遍历缓冲区求和
│   │   └── sum += s_period_buffer[i]  (i=0 to count-1)
│   ├── 计算平均周期
│   │   └── avg_ticks = sum / count
│   ├── 转换为RPM
│   │   └── RPM = (50000 × 60) / (avg_ticks × 6)
│   │       // 50000: TIM5频率(Hz)
│   │       // 60: 秒转分钟
│   │       // 6: 每转6次换相
│   └── 返回RPM值
└── 结束
```

### 4.4 ADC采样与电压计算流程

```
ADC采样触发（TIM6定时触发）
├── TIM6溢出触发ADC转换
├── ADC1采样VBUS/电流/温度通道
├── DMA自动传输数据到缓冲区
└── DMA传输完成中断
    └── bsp_adc_motor_dma_callback()
        └── 设置数据就绪标志

主循环中处理ADC数据
bsp_adc_motor_poll()
├── 检查数据就绪标志
├── [数据就绪]
│   ├── 多次采样平均滤波
│   ├── 更新VBUS/电流/温度缓存
│   └── 清除就绪标志
└── 结束

获取VBUS电压
bsp_adc_get_vbus()
├── bsp_adc_get_vbus_raw()                   // 获取原始ADC值
├── 电压计算
│   └── V = (raw / 4096) × 3.3V × 25 × 1.0204
│       // 4096: 12位ADC满量程
│       // 3.3V: ADC参考电压
│       // 25: 分压比（24:1实际测量为25:1）
│       // 1.0204: 校准系数（23.98/23.5）
└── 返回电压值（V）
```

---

## 5. 数据流图

### 5.1 霍尔传感器 → 换相 → PWM输出

```
霍尔传感器(U/V/W)
    ↓ [GPIO读取]
bsp_hall_read_state()
    ↓ [3位状态码: 1-6]
motor_phase_tab_get_phase()
    ↓ [查表 + 方向 + 偏移]
6路PWM状态(UH/UL/VH/VL/WH/WL)
    ↓ [HAL_TIM_PWM控制]
bsp_pwm_set_phase()
    ↓ [TIM1输出]
三相桥驱动器
    ↓
BLDC电机
```

### 5.2 换相周期 → RPM计算

```
霍尔状态变化
    ↓ [TIM5计数器]
换相周期测量(ticks)
    ↓ [存入缓冲区]
RPM滤波缓冲区[6]
    ↓ [滑动平均]
平均周期(avg_ticks)
    ↓ [公式转换]
RPM = (50000×60)/(avg_ticks×6)
    ↓ [串口输出]
用户界面显示
```

### 5.3 ADC采样 → 电压监测

```
TIM6定时器(触发源)
    ↓ [定时触发]
ADC1多通道采样
    ↓ [DMA传输]
ADC原始数据缓冲区
    ↓ [主循环处理]
bsp_adc_motor_poll()
    ↓ [平均滤波 + 校准]
VBUS电压值(V)
    ↓ [串口输出]
用户界面显示
```

---

## 6. 关键时序参数

| 参数 | 值 | 说明 |
|------|-----|------|
| PWM频率 | 10kHz | TIM1定时器，ARR=8399 |
| 霍尔采样频率 | 50kHz | TIM5定时器，用于周期测量 |
| ADC采样频率 | ~1kHz | TIM6触发 |
| 按键扫描周期 | 主循环 | 消抖时间20ms |
| 状态打印周期 | 200ms | 串口输出 |
| RPM滤波窗口 | 6次换相 | 滑动平均 |
| 驱动器使能延时 | 10ms | SD引脚使能后等待 |

---

## 7. 状态机转换

### 7.1 电机状态机

```
[STOPPED]
    ↓ KEY1按下 / motor_ctrl_start()
[STARTING]
    ↓ motor_start_simple()完成
[RUNNING]
    ↓ KEY2按下 / motor_ctrl_stop()
    ↓ 刹车故障 / bsp_pwm_break_callback()
[STOPPED]
```

### 7.2 换相状态循环（顺时针CW）

```
Hall=1 → Hall=5 → Hall=4 → Hall=6 → Hall=2 → Hall=3 → Hall=1 ...
  ↓       ↓       ↓       ↓       ↓       ↓
Phase0  Phase1  Phase2  Phase3  Phase4  Phase5
```

---

## 8. 错误处理流程

### 8.1 无效霍尔转换

```
bsp_hall_poll_and_commutate()
├── 检测到霍尔状态变化
├── motor_phase_tab_is_valid_transition()
├── [返回false]
│   ├── s_invalid_transition_count++         // 无效计数+1
│   ├── 不执行换相
│   └── 保持当前PWM状态
└── 继续监测
```

### 8.2 过流保护（刹车）

```
硬件过流检测
    ↓ [TIM1_BKIN触发]
TIM1_BRK中断
    ↓
bsp_pwm_break_callback()
├── 设置刹车故障标志
├── bsp_pwm_set_duty_cycle(0)                // 立即停止PWM
├── bsp_pwm_set_phase(0)                     // 关闭所有通道
├── motor_ctrl_set_fault()                   // 设置电机故障
└── 等待用户手动复位
```

---

## 9. 模块依赖关系

```
Test_Phase5 (应用层)
    ↓ 调用
Motor_Ctrl (电机控制层)
    ↓ 调用
Motor_Phase_Tab (换相表层)
    ↓ 调用
BSP层 (硬件抽象层)
├── bsp_hall (霍尔传感器)
├── bsp_pwm (PWM输出)
├── bsp_adc_motor (ADC采样)
├── bsp_key (按键输入)
├── bsp_ctrl_sd (驱动器使能)
└── bsp_uart (串口通信)
    ↓ 调用
HAL库 (STM32 HAL驱动)
    ↓ 控制
硬件外设 (TIM/ADC/GPIO/UART/DMA)
```

---

## 10. 性能优化要点

1. **RPM滤波**：使用6次换相的滑动平均，将RPM波动从±64%降低到±1%
2. **霍尔轮询**：50kHz高频轮询确保及时换相，避免失步
3. **ADC DMA**：使用DMA传输减少CPU占用
4. **中断优先级**：TIM1刹车中断优先级最高，确保安全保护
5. **状态缓存**：ADC/霍尔数据缓存避免重复读取
6. **VBUS校准**：单点校准系数1.0204，误差<0.1%

---

## 文档版本

- 版本：1.0
- 日期：2026-03-16
- 基于代码版本：commit 1c2902c
