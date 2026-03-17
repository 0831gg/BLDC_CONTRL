# test.c 文件框架分析

## 文件概述

**文件路径**: `User/Test/Src/test.c`
**功能**: BLDC电机控制测试主程序
**架构**: 时间片轮询 + 状态机

---

## 整体架构

```
test.c (BLDC电机控制测试主程序)
├── 配置参数 (lines 16-28)
├── 私有变量 (lines 30-39)
├── 辅助函数 (lines 42-245)
│   ├── 工具函数 (lines 45-78)
│   ├── 状态显示 (lines 80-118)
│   ├── 电机控制 (lines 120-191)
│   └── 事件处理 (lines 193-245)
└── 公共接口 (lines 247-381)
    ├── Test_Init() - 初始化
    └── Test_Loop() - 主循环
```

---

## 详细模块分解

### 1. 配置参数区 (lines 16-28)

```c
/* 时间间隔配置 */
#define LED_INTERVAL_MS       200U    // LED闪烁周期
#define KEY_INTERVAL_MS       20U     // 按键扫描周期
#define PRINT_INTERVAL_MS     800U    // 状态打印周期

/* 占空比表：0%, 5%, 10%, 15%...95% (步进5%) */
static const uint8_t DUTY_PERCENT_TABLE[] = {
    0, 5, 10, 15, 20, 25, 30, 35, 40, 45, 50,
    55, 60, 65, 70, 75, 80, 85, 90, 95
};
#define DUTY_TABLE_SIZE  (sizeof(DUTY_PERCENT_TABLE) / sizeof(DUTY_PERCENT_TABLE[0]))
```

**说明**:
- 定义了3个时间片任务的执行周期
- 占空比表支持20档调节（0%-95%）

---

### 2. 私有变量区 (lines 30-39)

| 变量名 | 类型 | 说明 |
|--------|------|------|
| `s_key` | `KeyGroup_HandleTypeDef` | 按键组句柄 |
| `s_led_tick` | `uint32_t` | LED更新时间戳 |
| `s_key_tick` | `uint32_t` | 按键扫描时间戳 |
| `s_print_tick` | `uint32_t` | 状态打印时间戳 |
| `s_duty_index` | `uint8_t` | 当前占空比索引（0-19） |
| `s_init_ok` | `uint8_t` | 初始化完成标志 |

---

### 3. 辅助函数区 (lines 42-245)

#### 3.1 工具函数

##### `get_current_duty()` (line 48)
```c
static inline uint16_t get_current_duty(void)
```
- **功能**: 将占空比百分比转换为PWM计数值
- **返回**: PWM计数值（0 - BSP_PWM_PERIOD）
- **公式**: `(BSP_PWM_PERIOD + 1) * percent / 100`

##### `format_hall_state()` (line 58)
```c
static void format_hall_state(uint8_t state, char *buf)
```
- **功能**: 将霍尔状态（0-7）格式化为"101"形式的字符串
- **参数**:
  - `state`: 霍尔状态（bit2=H1, bit1=H2, bit0=H3）
  - `buf`: 输出缓冲区（至少4字节）
- **示例**: `state=5` → `"101"`

##### `update_led()` (line 69)
```c
static void update_led(void)
```
- **功能**: LED状态指示
  - 电机运行时：LED0闪烁，LED1熄灭
  - 电机停止时：LED1闪烁，LED0熄灭

---

#### 3.2 状态显示

##### `print_status()` (line 83)
```c
static void print_status(void)
```
- **功能**: 打印完整的系统运行状态
- **输出内容**:
  1. 电机状态：运行/停止、方向、霍尔状态、占空比、转速
  2. 电源状态：母线电压（VBUS）、芯片温度
  3. 电流状态：三相电流（Iu/Iv/Iw）、最大相电流
  4. 过流警告（如果检测到）

**输出示例**:
```
[P5] Run=1 Dir=CCW Hall=101 Duty=20% RPM=1234
     VBUS=24.000V Temp=35.2C
     Iu=123.4mA Iv=234.5mA Iw=345.6mA Imax=345.6mA
```

---

#### 3.3 电机控制

##### `start_motor()` (line 123)
```c
static void start_motor(void)
```
- **功能**: 启动电机
- **流程**:
  1. 获取当前占空比
  2. 调用 `motor_start_simple(duty, direction)`
  3. 打印启动结果

##### `stop_motor()` (line 137)
```c
static void stop_motor(void)
```
- **功能**: 停止电机
- **流程**:
  1. 调用 `motor_stop()`
  2. 打印停止信息

##### `toggle_direction()` (line 146)
```c
static void toggle_direction(void)
```
- **功能**: 切换电机旋转方向（CW ↔ CCW）
- **流程**:
  1. 如果电机正在运行，先停止
  2. 切换方向（CCW → CW 或 CW → CCW）
  3. 如果之前在运行，重新启动

##### `adjust_duty()` (line 166)
```c
static void adjust_duty(void)
```
- **功能**: 调整占空比（循环递增）
- **流程**:
  1. 索引递增（循环：0→1→...→19→0）
  2. 更新PWM占空比
  3. 打印新占空比

##### `adjust_offset()` (line 176)
```c
static void adjust_offset(void)
```
- **功能**: 调整换相偏移（0-5）
- **流程**:
  1. 如果电机正在运行，先停止
  2. 调整偏移量（循环：0→1→...→5→0）
  3. 如果之前在运行，重新启动

---

#### 3.4 事件处理

##### `handle_keys()` (line 196)
```c
static void handle_keys(void)
```
- **功能**: 处理按键事件

| 按键 | 事件 | 功能 |
|------|------|------|
| KEY0 | 短按 | 启动/停止电机 |
| KEY1 | 短按 | 切换旋转方向 |
| KEY2 | 短按 | 调整占空比（+5%） |
| KEY2 | 长按 | 调整换相偏移 |

##### `handle_faults()` (line 230)
```c
static void handle_faults(void)
```
- **功能**: 处理系统故障
- **故障类型**:
  1. **硬件故障**（Break Fault）
     - 检测：`bsp_pwm_is_break_fault()`
     - 处理：停止电机，设置故障标志，清除故障
  2. **霍尔无效故障**
     - 检测：`motor_ctrl_get_fault() == MOTOR_FAULT_HALL_INVALID`
     - 处理：停止电机，打印警告

---

### 4. 公共接口 (lines 247-381)

#### 4.1 Test_Init() (line 254)

```c
void Test_Init(void)
```

**功能**: 系统初始化

**初始化流程**:

```
1. 基础模块初始化
   ├── Delay_Init()          - 延时函数
   ├── BSP_UART_Init()       - UART调试输出
   ├── VOFA_Init()           - VOFA+协议（关键！）
   ├── LED_Init()            - LED指示
   └── Key_Init()            - 按键扫描

2. 硬件外设初始化
   ├── bsp_ctrl_sd_init()    - SD控制（驱动使能）
   ├── bsp_pwm_init()        - PWM生成
   ├── bsp_hall_init()       - 霍尔传感器
   ├── bsp_adc_motor_init()  - ADC（电压/温度）
   └── HAL_TIM_PWM_Start()   - ADC触发定时器

3. 电流采样初始化
   └── bsp_current_sense_init() - 校准电流传感器零点

4. 电机控制初始化
   ├── motor_ctrl_init()        - 电机控制逻辑
   └── bsp_pwm_disable_break()  - 禁用硬件保护（测试模式）

5. 打印配置信息
   ├── 系统时钟频率
   ├── PWM频率
   ├── 按键功能说明
   └── 占空比范围
```

**错误处理**:
- 如果任何硬件初始化失败，点亮LED0和LED1，停止初始化

**输出示例**:
```
=== BLDC Motor Control Test (170MHz) ===
SYSCLK: 170 MHz

Initializing hardware...
  SD Control: OK
  PWM: OK
  Hall: OK
  ADC: OK
  ADC Trigger: OK

Calibrating current sensors...
  Current Sense: OK (U=2048 V=2048 W=2048)
Motor Control: OK
Break Protection: Disabled (Test Mode)

=== Configuration ===
PWM Frequency: 20.00 kHz
KEY0: Start/Stop
KEY1: Direction
KEY2: Duty/Offset
Duty Range: 8%-80%, Step: 5%

=== System Ready ===
```

---

#### 4.2 Test_Loop() (line 342)

```c
void Test_Loop(void)
```

**功能**: 主循环（时间片轮询）

**任务调度**:

| 任务 | 周期 | 函数 | 说明 |
|------|------|------|------|
| LED更新 | 200ms | `update_led()` | 状态指示 |
| 按键扫描 | 20ms | `Key_Scan()` + `handle_keys()` | 用户输入 |
| 电流滤波 | 每次循环 | `bsp_current_sense_update_filter()` | 实时滤波 |
| 故障处理 | 每次循环 | `handle_faults()` | 安全保护 |
| 过流保护 | 每次循环 | 检测过流并停机 | 软件保护 |
| 状态打印 | 800ms | `print_status()` | 调试输出 |

**执行流程**:
```c
while(1) {
    uint32_t now = HAL_GetTick();

    // 1. LED更新 (200ms)
    if ((now - s_led_tick) >= 200) {
        update_led();
    }

    // 2. 按键扫描 (20ms)
    if ((now - s_key_tick) >= 20) {
        Key_Scan();
        handle_keys();
    }

    // 3. 电流滤波更新 (每次)
    bsp_current_sense_update_filter();

    // 4. 故障处理 (每次)
    handle_faults();

    // 5. 过流保护 (每次)
    if (overcurrent && running) {
        stop_motor();
    }

    // 6. 状态打印 (800ms)
    if ((now - s_print_tick) >= 800) {
        print_status();
    }
}
```

---

## 依赖关系

### BSP模块依赖

```
test.c
├── bsp_adc_motor.h       - ADC采样（母线电压、芯片温度）
├── bsp_current_sense.h   - 电流采样、滤波、过流检测
├── bsp_ctrl_sd.h         - SD控制（驱动使能/禁用）
├── bsp_delay.h           - 延时函数
├── bsp_hall.h            - 霍尔传感器（状态读取、转速计算）
├── bsp_key.h             - 按键扫描（短按/长按检测）
├── bsp_led.h             - LED控制（状态指示）
├── bsp_pwm.h             - PWM生成、故障检测
├── bsp_uart.h            - UART调试输出
├── motor_ctrl.h          - 电机控制逻辑（启停、方向、换相）
├── tim.h                 - 定时器句柄（htim1）
└── vofa_uart.h           - VOFA+协议（数据可视化）
```

### 调用关系图

```
Test_Init()
├── Delay_Init()
├── BSP_UART_Init()
├── VOFA_Init()
├── LED_Init()
├── Key_Init()
├── bsp_ctrl_sd_init()
├── bsp_pwm_init()
├── bsp_hall_init()
├── bsp_adc_motor_init()
├── bsp_current_sense_init()
├── motor_ctrl_init()
└── bsp_pwm_disable_break()

Test_Loop()
├── update_led()
│   ├── motor_ctrl_is_running()
│   ├── LED0_TOGGLE() / LED1_TOGGLE()
│   └── LED0_OFF() / LED1_OFF()
├── Key_Scan()
├── handle_keys()
│   ├── start_motor() / stop_motor()
│   ├── toggle_direction()
│   ├── adjust_duty()
│   └── adjust_offset()
├── bsp_current_sense_update_filter()
├── handle_faults()
│   ├── bsp_pwm_is_break_fault()
│   └── motor_ctrl_get_fault()
├── bsp_current_sense_is_overcurrent()
└── print_status()
    ├── bsp_hall_get_state()
    ├── bsp_adc_get_vbus()
    ├── bsp_adc_get_temp()
    ├── bsp_current_sense_get_filtered()
    └── motor_ctrl_is_running()
```

---

## 关键特性

### 1. 时间片轮询架构
- 使用 `HAL_GetTick()` 实现非阻塞多任务
- 不同任务按不同周期执行
- 避免使用延时函数阻塞主循环

### 2. 状态机设计
- 电机启停状态切换
- 方向切换（需停机）
- 占空比动态调整
- 换相偏移调整（需停机）

### 3. 多层故障保护
- **硬件保护**: PWM Break（过流硬件触发）
- **软件保护**: 电流采样过流检测
- **逻辑保护**: 霍尔无效检测

### 4. 实时监控
- 三相电流（滤波后）
- 母线电压
- 芯片温度
- 电机转速
- 霍尔状态

### 5. 用户交互
- **按键控制**: 3个按键，4种功能
- **LED指示**: 运行/停止状态
- **UART输出**: 详细调试信息
- **VOFA+协议**: 数据可视化

---

## 使用说明

### 初始化
```c
int main(void) {
    HAL_Init();
    SystemClock_Config();

    Test_Init();  // 初始化所有模块

    while(1) {
        Test_Loop();  // 主循环
    }
}
```

### 按键操作

1. **启动电机**
   - 按下 KEY0
   - 电机以当前占空比和方向启动
   - LED0开始闪烁

2. **调整占空比**
   - 短按 KEY2
   - 占空比递增5%（循环）
   - 实时生效（无需停机）

3. **切换方向**
   - 短按 KEY1
   - 自动停机→切换方向→重启
   - 方向：CCW（逆时针）↔ CW（顺时针）

4. **调整换相偏移**
   - 长按 KEY2
   - 自动停机→调整偏移→重启
   - 偏移量：0→1→...→5→0

5. **停止电机**
   - 再次按下 KEY0
   - LED1开始闪烁

---

## 注意事项

### 安全相关
1. **测试模式**: `bsp_pwm_disable_break()` 禁用了硬件保护，仅用于测试
2. **过流保护**: 软件过流检测仍然有效
3. **故障处理**: 检测到故障会自动停机

### 性能相关
1. **电流滤波**: 每次循环更新，需要足够的采样率
2. **按键扫描**: 20ms周期，支持防抖和长按检测
3. **状态打印**: 800ms周期，避免UART阻塞

### 调试相关
1. **VOFA初始化**: 必须调用 `VOFA_Init()`，否则VOFA无法工作
2. **UART波特率**: 1000000 (1Mbps)，需要上位机支持
3. **LED指示**: 初始化失败时LED0和LED1同时点亮

---

## 总结

`test.c` 是整个BLDC电机控制系统的顶层测试框架，具有以下特点：

- **模块化设计**: 清晰的功能分层
- **非阻塞架构**: 时间片轮询，实时性好
- **完善的保护**: 多层故障检测与处理
- **易于调试**: 丰富的状态输出和可视化支持
- **用户友好**: 简单的按键操作，直观的LED指示

该框架适合用于电机控制系统的开发、测试和调试。
