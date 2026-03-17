# VESC电机参数辨识方案

## 目录
1. [概述](#概述)
2. [辨识参数说明](#辨识参数说明)
3. [硬件要求](#硬件要求)
4. [CubeMX配置要求](#cubemx配置要求)
5. [实现路径与文件结构](#实现路径与文件结构)
6. [参数辨识算法](#参数辨识算法)
7. [实施步骤](#实施步骤)
8. [与现有代码集成](#与现有代码集成)

---

## 概述

本文档基于开源VESC（Vedder Electronic Speed Controller）方案，为您的BLDC电机控制项目提供参数辨识实现指南。VESC采用自动化测试序列来识别电机的电气和机械参数，无需手动测量。

### VESC参数辨识核心思想
- **电阻辨识**：施加直流电压，测量稳态电流
- **电感辨识**：施加阶跃电压，测量电流上升率
- **磁链辨识**：开环旋转电机，测量反电动势
- **惯量辨识**：施加已知转矩，测量加速度

---

## 辨识参数说明

### 电气参数
| 参数 | 符号 | 单位 | 说明 |
|------|------|------|------|
| 定子电阻 | Rs | Ω | 相电阻，影响铜损和电流控制 |
| d轴电感 | Ld | H | 直轴电感（对于表贴式BLDC，Ld≈Lq） |
| q轴电感 | Lq | H | 交轴电感 |
| 反电动势系数 | Ke | V/(rad/s) | 电压常数 |
| 磁链 | ψf | Wb | 永磁体磁链 |

### 机械参数
| 参数 | 符号 | 单位 | 说明 |
|------|------|------|------|
| 转动惯量 | J | kg·m² | 转子惯量 |
| 摩擦系数 | B | N·m·s | 粘滞摩擦系数 |

---

## 硬件要求

### 当前硬件能力（已具备）
✅ **电流采样**
- 三相电流采样（ADC2注入通道）
- 采样电阻：0.02Ω
- 运放增益：6.0 V/V
- 分辨率：6.71 mA/LSB
- 文件：`bsp_current_sense.h/c`

✅ **电压采样**
- 母线电压采样（ADC1）
- 分压比：25:1
- 文件：`bsp_adc_motor.h/c`

✅ **PWM控制**
- TIM1三相PWM输出
- 频率：10kHz
- 占空比分辨率：8400级
- 文件：`bsp_pwm.h/c`

✅ **霍尔传感器**
- 三路霍尔位置反馈
- 用于速度测量和换相
- 文件：`bsp_hall.h/c`

### 需要补充的功能
⚠️ **高精度时间测量**
- 用于电感辨识时测量di/dt
- 建议使用TIM2/TIM5（32位定时器）

⚠️ **速度测量**
- 基于霍尔传感器的转速计算
- 需要实现霍尔周期测量

---

## CubeMX配置要求

### 1. ADC配置（已配置）

**ADC1 - 母线电压和温度采样**
```
Mode: Independent mode
Clock Prescaler: PCLK2 divided by 2
Resolution: 12 bits
Data Alignment: Right alignment
Scan Conversion Mode: Enabled
Continuous Conversion Mode: Enabled
DMA Continuous Requests: Enabled

Regular Channels:
- Channel 1 (VBUS): Rank 1, Sampling time 480 cycles
- Channel 2 (TEMP): Rank 2, Sampling time 480 cycles

DMA Settings:
- DMA2 Stream 0
- Direction: Peripheral to Memory
- Mode: Circular
- Data Width: Half Word (16-bit)
```

**ADC2 - 三相电流采样**
```
Mode: Independent mode
Resolution: 12 bits
Injected Channels:
- Channel 1 (IU): Rank 1, Sampling time 15 cycles
- Channel 2 (IV): Rank 2, Sampling time 15 cycles
- Channel 3 (IW): Rank 3, Sampling time 15 cycles

Trigger: TIM1 TRGO (与PWM同步)
Interrupt: Enabled (End of Injected Conversion)
```

### 2. TIM1配置（已配置）

**PWM生成**
```
Clock Source: Internal Clock (84MHz)
Prescaler: 0
Counter Mode: Up
Counter Period: 8399 (10kHz PWM)
Auto-reload preload: Enabled

Channel 1 (U相): PWM Generation CH1, PE9
Channel 2 (V相): PWM Generation CH2, PE11
Channel 3 (W相): PWM Generation CH3, PE13

PWM Mode: PWM mode 1
Pulse: 0 (初始占空比)
Output Compare Preload: Enabled
Fast Mode: Disabled
CH Polarity: High

Break and Dead Time:
- Break Input: Enabled (过流保护)
- Dead Time: 根据驱动芯片要求设置
```

### 3. TIM2/TIM5配置（需要新增）

**用于高精度时间测量**
```
推荐使用TIM2或TIM5（32位定时器）

Clock Source: Internal Clock (84MHz)
Prescaler: 0 (1MHz计数频率，1μs分辨率)
Counter Mode: Up
Counter Period: 0xFFFFFFFF (最大值)
Auto-reload preload: Disabled

用途：
- 测量电流上升时间（电感辨识）
- 测量霍尔周期（速度计算）
```

### 4. GPIO配置（已配置）

**下桥臂控制**
```
PB13 (UN): GPIO_Output, Pull-down, Low
PB14 (VN): GPIO_Output, Pull-down, Low
PB15 (WN): GPIO_Output, Pull-down, Low
```

**霍尔传感器**
```
Hall_U: GPIO_Input, Pull-up
Hall_V: GPIO_Input, Pull-up
Hall_W: GPIO_Input, Pull-up
```

### 5. UART配置（可选，用于调试）

**VOFA+数据可视化**
```
已有vofa_uart模块，可用于实时监控辨识过程
Baud Rate: 115200
Data Bits: 8
Stop Bits: 1
Parity: None
```

---

## 实现路径与文件结构

### 推荐的文件组织

```
User/
├── Motor/
│   ├── Inc/
│   │   ├── motor_ctrl.h              (已存在)
│   │   ├── motor_phase_tab.h         (已存在)
│   │   └── motor_param_id.h          (新建) ← 参数辨识接口
│   └── Src/
│       ├── motor_ctrl.c              (已存在)
│       ├── motor_phase_tab.c         (已存在)
│       └── motor_param_id.c          (新建) ← 参数辨识实现
│
├── Bsp/
│   ├── Inc/
│   │   ├── bsp_current_sense.h       (已存在)
│   │   ├── bsp_adc_motor.h           (已存在)
│   │   ├── bsp_pwm.h                 (已存在)
│   │   ├── bsp_hall.h                (已存在)
│   │   └── bsp_timer_measure.h       (新建) ← 时间测量工具
│   └── Src/
│       ├── bsp_current_sense.c       (已存在)
│       ├── bsp_adc_motor.c           (已存在)
│       ├── bsp_pwm.c                 (已存在)
│       ├── bsp_hall.c                (已存在)
│       └── bsp_timer_measure.c       (新建) ← 时间测量实现
│
└── Test/
    └── Src/
        └── test.c                     (已存在) ← 在此调用辨识流程
```

### 文件功能说明

#### 新建文件1：`motor_param_id.h/c`
**功能**：电机参数辨识主模块
- 辨识状态机管理
- 各参数辨识算法封装
- 结果存储和查询接口

**主要函数**：
```c
// 初始化辨识模块
uint8_t motor_param_id_init(void);

// 执行完整辨识流程
uint8_t motor_param_id_run_full(void);

// 单独辨识各参数
uint8_t motor_param_id_resistance(void);
uint8_t motor_param_id_inductance(void);
uint8_t motor_param_id_flux_linkage(void);
uint8_t motor_param_id_inertia(void);

// 获取辨识结果
void motor_param_id_get_results(motor_params_t *params);
```

#### 新建文件2：`bsp_timer_measure.h/c`
**功能**：高精度时间测量工具
- 基于TIM2/TIM5的微秒级计时
- 用于电流上升时间测量

**主要函数**：
```c
// 初始化测量定时器
void bsp_timer_measure_init(void);

// 开始计时
void bsp_timer_measure_start(void);

// 停止并返回时间（微秒）
uint32_t bsp_timer_measure_stop_us(void);

// 获取当前计数值
uint32_t bsp_timer_measure_get_count(void);
```

---

## 参数辨识算法

### 1. 定子电阻辨识（Rs）

**原理**：欧姆定律 R = V / I

**步骤**：
1. 电机静止，施加低压直流电压（10-20% PWM）
2. 等待电流稳定（约100ms）
3. 测量稳态电压和电流
4. 计算：Rs = Vdc / Idc

**实现要点**：
```
使用函数：
- bsp_pwm_duty_set()           设置PWM占空比
- bsp_pwm_ctrl_single_phase()  选择通电相
- bsp_pwm_lower_set()          设置下桥臂
- bsp_current_sense_get_filtered()  读取电流
- bsp_adc_get_vbus()           读取母线电压

测试配置：
- 占空比：15% (约1260/8400)
- 通电相：U相上桥臂 + V相下桥臂
- 稳定时间：100ms
- 采样次数：100次取平均

计算公式：
Vphase = Vbus × duty_cycle
Rs = Vphase / I_measured
```

**注意事项**：
- 电流不要太大（<1A），避免发热影响
- 多次测量取平均值
- 补偿导线电阻和MOSFET导通压降

### 2. 定子电感辨识（Ls）

**原理**：L = V × dt / dI

**步骤**：
1. 电机静止，初始电流为0
2. 施加阶跃电压（30-50% PWM）
3. 测量电流从I1上升到I2的时间dt
4. 计算：L = (V - I×Rs) × dt / (I2 - I1)

**实现要点**：
```
使用函数：
- bsp_timer_measure_start()    开始计时
- bsp_timer_measure_stop_us()  停止计时
- bsp_current_sense_get_raw()  快速读取电流

测试配置：
- 占空比：40% (约3360/8400)
- 通电相：U相上桥臂 + V相下桥臂
- 电流阈值：I1 = 500mA, I2 = 1500mA
- 采样频率：10kHz（ADC注入中断）

计算公式：
V_applied = Vbus × duty - I_avg × Rs
L = V_applied × dt / (I2 - I1)

其中：
- dt: 电流上升时间（秒）
- I_avg: (I1 + I2) / 2
```

**注意事项**：
- 使用原始电流值，不要滤波（保持快速响应）
- 补偿电阻压降
- 测量区间选择线性段（避免饱和区）

### 3. 磁链/反电动势系数辨识（Ke, ψf）

**原理**：开环旋转电机，测量反电动势

**步骤**：
1. 开环强制换相，使电机旋转
2. 逐步提高转速至稳定值
3. 断开PWM，测量反电动势波形
4. 计算：Ke = Vemf / ω

**实现要点**：
```
使用函数：
- motor_sensor_mode_phase()    强制换相
- bsp_hall_get_speed_rpm()     测量转速
- bsp_adc_get_vbus()           测量母线电压
- bsp_current_sense_get_filtered()  测量电流

测试配置：
阶段1 - 加速：
- 初始占空比：20%
- 换相频率：从10Hz逐步增加到目标频率
- 加速时间：2-3秒
- 目标转速：500-1000 RPM

阶段2 - 稳速：
- 保持恒定换相频率
- 持续时间：1秒
- 记录：转速ω、电流I、电压V

阶段3 - 测量反电动势：
- 关闭PWM输出
- 电机惯性滑行
- 测量相电压（通过ADC或比较器）
- 记录反电动势峰值Vemf

计算公式：
ω = RPM × 2π / 60 × pole_pairs  (电角速度)
Ke = Vemf_peak / ω
ψf = Ke / sqrt(3)  (线反电动势转相反电动势)
```

**注意事项**：
- 需要电机能够自由旋转（无负载或轻负载）
- 反电动势测量需要快速ADC采样或比较器
- 如果硬件不支持反电动势测量，可使用功率平衡法：
  ```
  P_mech = P_elec - P_loss
  T × ω = V × I - I² × Rs
  Ke = T / I = (V × I - I² × Rs) / (I × ω)
  ```

### 4. 转动惯量辨识（J）

**原理**：牛顿第二定律 J = T / α

**步骤**：
1. 电机加速，施加恒定转矩
2. 测量角加速度
3. 计算：J = T / α

**实现要点**：
```
使用函数：
- motor_start_simple()         启动电机
- motor_ctrl_set_duty()        设置转矩（占空比）
- bsp_hall_get_speed_rpm()     测量转速

测试配置：
- 占空比：30% (恒定)
- 测量时间：1秒
- 采样频率：100Hz（每10ms记录一次转速）

计算公式：
1. 记录转速序列：ω(t)
2. 线性拟合得到角加速度：α = dω/dt
3. 估算转矩：T = Kt × I_avg
   其中 Kt ≈ Ke (对于BLDC电机)
4. 计算惯量：J = T / α

数据处理：
- 使用最小二乘法拟合ω(t)
- 剔除启动瞬态（前200ms）
- 补偿摩擦转矩：T_net = T_motor - B×ω
```

**注意事项**：
- 需要先完成Ke辨识
- 测试时负载应恒定
- 多次测试取平均值
- 如果有负载，需要分离负载惯量

### 5. 摩擦系数辨识（B）

**原理**：稳态时 T_friction = B × ω

**步骤**：
1. 电机恒速运行
2. 测量维持该速度所需的转矩
3. 多个速度点测量
4. 线性拟合得到B

**实现要点**：
```
测试配置：
- 速度点：200, 400, 600, 800, 1000 RPM
- 每个速度点稳定时间：2秒
- 记录：转速ω、电流I

计算公式：
T = Kt × I
B = T / ω

数据处理：
- 对多个(ω, T)点进行线性拟合
- 斜率即为摩擦系数B
- 截距为库仑摩擦
```

---

## 实施步骤

### 步骤1：准备工作

1. **硬件检查**
   - 确认电机可自由旋转
   - 确认电流传感器已校准（调用`bsp_current_sense_calibrate()`）
   - 确认母线电压稳定（12V或24V）
   - 确认驱动器无故障

2. **软件准备**
   - 在CubeMX中配置TIM2/TIM5（如果尚未配置）
   - 创建`bsp_timer_measure.h/c`文件
   - 创建`motor_param_id.h/c`文件
   - 在Keil工程中添加新文件

### 步骤2：实现底层测量工具

**文件：`bsp_timer_measure.c`**

实现内容：
- TIM2/TIM5初始化（1MHz计数频率）
- 开始/停止计时函数
- 微秒级时间读取

关键代码逻辑：
```
初始化：
- 使能TIM2时钟
- 配置预分频器：PSC = 84-1 (1MHz)
- 配置自动重载：ARR = 0xFFFFFFFF
- 启动定时器

计时：
- start(): 清零计数器，开始计数
- stop(): 读取计数器值，转换为微秒
```

### 步骤3：实现参数辨识算法

**文件：`motor_param_id.c`**

实现内容：
- 定义参数结构体存储结果
- 实现各参数辨识函数
- 实现完整辨识流程

数据结构：
```c
typedef struct {
    float rs_ohm;           // 定子电阻
    float ls_henry;         // 定子电感
    float ke_v_rad_s;       // 反电动势系数
    float flux_linkage_wb;  // 磁链
    float inertia_kg_m2;    // 转动惯量
    float friction_nm_s;    // 摩擦系数
    uint8_t valid;          // 辨识完成标志
} motor_params_t;
```

### 步骤4：集成到测试模块

**文件：`User/Test/Src/test.c`**

在现有测试框架中添加参数辨识命令：

```c
// 在test.c中添加辨识流程调用
void test_motor_param_identification(void)
{
    motor_params_t params;

    printf("开始电机参数辨识...\r\n");

    // 执行完整辨识
    if (motor_param_id_run_full() == 0) {
        // 获取结果
        motor_param_id_get_results(&params);

        // 打印结果
        printf("辨识完成！\r\n");
        printf("Rs = %.4f Ω\r\n", params.rs_ohm);
        printf("Ls = %.6f H\r\n", params.ls_henry);
        printf("Ke = %.4f V/(rad/s)\r\n", params.ke_v_rad_s);
        printf("ψf = %.6f Wb\r\n", params.flux_linkage_wb);
        printf("J  = %.8f kg·m²\r\n", params.inertia_kg_m2);
        printf("B  = %.6f N·m·s\r\n", params.friction_nm_s);
    } else {
        printf("辨识失败！\r\n");
    }
}
```

### 步骤5：调试与验证

1. **单步测试**
   - 先测试电阻辨识
   - 再测试电感辨识
   - 最后测试磁链和惯量

2. **结果验证**
   - 电阻：用万用表测量相电阻对比
   - 电感：查阅电机数据手册对比
   - Ke：计算Kv = 60/(2π×Ke×pole_pairs)，与标称Kv对比

3. **VOFA+可视化**
   - 使用现有vofa_uart模块
   - 实时显示电流、电压波形
   - 观察辨识过程

---

## 与现有代码集成

### 调用现有BSP函数

**电流采样**
```c
// 校准（电机停止时）
bsp_current_sense_calibrate(200);

// 读取原始电流（快速，用于电感辨识）
bsp_current_t current_raw;
bsp_current_sense_get_raw(&current_raw);

// 读取滤波电流（平滑，用于电阻辨识）
bsp_current_t current_filtered;
bsp_current_sense_get_filtered(&current_filtered);
```

**电压采样**
```c
// 读取母线电压
float vbus = bsp_adc_get_vbus();
```

**PWM控制**
```c
// 设置占空比
bsp_pwm_duty_set(1260);  // 15%占空比

// 控制单相PWM
bsp_pwm_ctrl_single_phase(BSP_PWM_PHASE_U);

// 设置下桥臂
bsp_pwm_lower_set(0, 1, 0);  // V相下桥臂导通
```

**电机控制**
```c
// 启动电机
motor_start_simple(2000, MOTOR_DIR_CW);

// 停止电机
motor_stop();

// 设置占空比
motor_ctrl_set_duty(3000);
```

**霍尔传感器**
```c
// 获取霍尔状态
uint8_t hall_state = motor_ctrl_get_hall_state();

// 获取转速（需要实现）
// uint16_t rpm = bsp_hall_get_speed_rpm();
```

### 需要补充的功能

**速度测量**（在`bsp_hall.c`中添加）
```c
// 基于霍尔周期计算转速
// 原理：测量相邻两次霍尔变化的时间间隔
// RPM = 60 / (period × 6 / pole_pairs)

uint16_t bsp_hall_get_speed_rpm(void)
{
    // 实现：
    // 1. 记录霍尔变化时间戳
    // 2. 计算周期
    // 3. 转换为RPM
}
```

### 数据输出

**VOFA+实时监控**
```c
// 在辨识过程中发送数据
float data[4] = {
    current_raw.u_ma,
    current_raw.v_ma,
    vbus,
    (float)timer_us
};
vofa_send_data(data, 4);
```

**串口打印**
```c
// 使用printf输出结果（需要重定向到UART）
printf("Rs = %.4f Ω\r\n", params.rs_ohm);
```

---

## 安全注意事项

### 电流限制
- 辨识过程中限制最大电流（<5A）
- 使用`bsp_current_sense_is_overcurrent()`检测过流
- 过流立即停止测试

### 温度监控
- 定期检查电机温度
- 使用`bsp_adc_get_temp()`读取温度
- 超温停止测试

### 故障处理
```c
// 在辨识循环中检查故障
if (bsp_current_sense_is_overcurrent()) {
    motor_stop();
    return -1;  // 过流故障
}

if (motor_ctrl_get_fault() != MOTOR_FAULT_NONE) {
    motor_stop();
    return -2;  // 电机故障
}
```

### 测试环境
- 电机应无负载或轻负载
- 确保电机可自由旋转
- 固定好电机，防止意外移动

---

## 参考资料

### VESC开源代码
- GitHub: https://github.com/vedderb/bldc
- 关键文件：
  - `mcpwm_foc.c` - FOC控制和参数辨识
  - `mc_interface.c` - 电机接口
  - `conf_general.c` - 参数配置

### 相关论文
- "Sensorless Control of BLDC Motor Based on Back-EMF Detection"
- "Parameter Identification for PMSM Based on Model Reference Adaptive System"

### 电机参数计算
- Kv与Ke关系：Kv (RPM/V) = 60 / (2π × Ke × pole_pairs)
- 转矩常数：Kt ≈ Ke (SI单位制)
- 功率：P = T × ω = Ke × I × ω

---

## 总结

本文档提供了基于VESC方案的电机参数辨识完整实现路径：

1. ✅ **硬件能力**：您的项目已具备电流、电压、PWM控制等基础功能
2. ⚙️ **CubeMX配置**：需要添加TIM2/TIM5用于高精度计时
3. 📁 **文件结构**：建议创建`motor_param_id.c`和`bsp_timer_measure.c`
4. 🧮 **算法实现**：详细说明了5个参数的辨识原理和步骤
5. 🔗 **代码集成**：说明了如何调用现有BSP函数和电机控制接口

**下一步行动**：
1. 在CubeMX中配置TIM2/TIM5
2. 创建新文件并实现底层测量工具
3. 实现参数辨识算法
4. 在test.c中集成测试流程
5. 使用VOFA+监控辨识过程
6. 验证辨识结果

祝您实现顺利！如有问题，请随时提问。
