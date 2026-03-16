# test_phase5.c 调用逻辑详解

## 概述

test_phase5.c是BLDC电机控制系统的测试主程序，负责：
- 系统初始化
- 按键交互
- 电机控制
- 状态监控
- 故障处理

---

## 1. 程序入口与初始化

### 1.1 调用流程图

```
main.c: main()
    ↓
Test_Phase5_Init()
    ├─ 基础外设初始化
    │   ├─ Delay_Init()              // 延时初始化
    │   ├─ BSP_UART_Init()           // 串口初始化
    │   ├─ LED_Init()                // LED初始化
    │   └─ Key_Init()                // 按键初始化
    │
    ├─ 电机硬件初始化
    │   ├─ bsp_ctrl_sd_init()        // 驱动器使能引脚初始化
    │   ├─ bsp_pwm_init()            // PWM初始化（TIM1）
    │   ├─ bsp_hall_init()           // 霍尔传感器初始化（TIM5）
    │   ├─ bsp_adc_motor_init()      // ADC初始化（VBUS/电流/温度）
    │   └─ HAL_TIM_PWM_Start()       // 启动ADC触发（TIM1_CH4）
    │
    ├─ 电机控制初始化
    │   ├─ motor_ctrl_init()         // 电机控制模块初始化
    │   └─ bsp_pwm_disable_break()   // 禁用硬件保护（测试用）
    │
    └─ 打印初始化信息
        └─ print_status()            // 打印初始状态
```

### 1.2 初始化代码详解

```c
void Test_Phase5_Init(void)
{
    /* 1. 基础初始化 */
    Delay_Init();                    // 初始化延时函数
    BSP_UART_Init();                 // 初始化UART1（1000000波特率）
    LED_Init();                      // 初始化LED0/LED1
    Key_Init(&s_key, ...);           // 初始化按键扫描

    /* 2. 硬件初始化（带错误检查） */
    if (bsp_hall_init() != 0) {
        // 霍尔初始化失败 → 点亮双LED → 退出
        LED0_ON(); LED1_ON();
        return;
    }

    if (bsp_adc_motor_init() != 0) {
        // ADC初始化失败 → 点亮双LED → 退出
        LED0_ON(); LED1_ON();
        return;
    }

    /* 3. 启动ADC触发定时器 */
    __HAL_TIM_SET_COMPARE(&htim1, TIM_CHANNEL_4, BSP_PWM_PERIOD / 2U);
    HAL_TIM_PWM_Start(&htim1, TIM_CHANNEL_4);

    /* 4. 电机控制初始化 */
    motor_ctrl_init();               // 初始化电机状态机
    bsp_pwm_disable_break();         // 测试时禁用硬件保护

    /* 5. 标记初始化完成 */
    s_init_ok = 1U;
    print_status();                  // 打印初始状态
}
```

---

## 2. 主循环调用逻辑

### 2.1 主循环流程图

```
main.c: while(1)
    ↓
Test_Phase5_Loop()
    ├─ 检查初始化状态
    │   └─ if (!s_init_ok) return;
    │
    ├─ LED更新 (200ms周期)
    │   └─ update_led()
    │       ├─ if (motor_ctrl_is_running())
    │       │   ├─ LED1_OFF()
    │       │   └─ LED0_TOGGLE()      // 运行时LED0闪烁
    │       └─ else
    │           ├─ LED0_OFF()
    │           └─ LED1_TOGGLE()      // 停止时LED1闪烁
    │
    ├─ 按键扫描与处理 (20ms周期)
    │   ├─ Key_Scan(&s_key)          // 扫描按键状态
    │   └─ handle_keys()             // 处理按键事件
    │       ├─ KEY0短按 → 启动/停止
    │       ├─ KEY1短按 → 切换方向
    │       ├─ KEY2短按 → 调整占空比
    │       └─ KEY2长按 → 调整换相偏移
    │
    ├─ 故障处理 (实时)
    │   └─ handle_faults()
    │       ├─ 检查硬件故障（过流）
    │       └─ 检查霍尔无效故障
    │
    └─ 状态打印 (800ms周期)
        └─ print_status()            // 打印运行状态
```

### 2.2 主循环代码详解

```c
void Test_Phase5_Loop(void)
{
    uint32_t now = HAL_GetTick();    // 获取当前时间戳

    /* 1. 检查初始化状态 */
    if (!s_init_ok) {
        return;  // 初始化失败，不执行后续逻辑
    }

    /* 2. LED更新（200ms周期） */
    if ((now - s_led_tick) >= LED_INTERVAL_MS) {
        s_led_tick = now;
        update_led();                // 根据运行状态更新LED
    }

    /* 3. 按键扫描（20ms周期） */
    if ((now - s_key_tick) >= KEY_INTERVAL_MS) {
        s_key_tick = now;
        Key_Scan(&s_key);            // 扫描按键（消抖）
        handle_keys();               // 处理按键事件
    }

    /* 4. 故障处理（实时） */
    handle_faults();                 // 检查并处理故障

    /* 5. 状态打印（800ms周期） */
    if ((now - s_print_tick) >= PRINT_INTERVAL_MS) {
        s_print_tick = now;
        print_status();              // 打印运行状态
    }
}
```

---

## 3. 按键处理逻辑

### 3.1 按键处理流程图

```
handle_keys()
    ├─ KEY0短按：启动/停止
    │   ├─ if (motor_ctrl_is_running())
    │   │   └─ stop_motor()
    │   │       ├─ motor_stop()
    │   │       │   ├─ bsp_pwm_set_duty_cycle(0)
    │   │       │   ├─ bsp_pwm_set_phase(0)
    │   │       │   └─ bsp_ctrl_sd_disable()
    │   │       └─ BSP_UART_Printf("Motor stopped")
    │   └─ else
    │       └─ start_motor()
    │           ├─ get_current_duty()
    │           ├─ motor_start_simple(duty, direction)
    │           │   ├─ bsp_ctrl_sd_enable()
    │           │   ├─ bsp_hall_read_state()
    │           │   ├─ motor_phase_tab_get_phase()
    │           │   ├─ bsp_pwm_set_phase()
    │           │   └─ bsp_pwm_set_duty_cycle()
    │           └─ BSP_UART_Printf("Motor started")
    │
    ├─ KEY1短按：切换方向
    │   └─ toggle_direction()
    │       ├─ 保存运行状态
    │       ├─ if (was_running) stop_motor()
    │       ├─ motor_ctrl_set_direction(new_dir)
    │       ├─ BSP_UART_Printf("Direction=...")
    │       └─ if (was_running) start_motor()
    │
    ├─ KEY2短按：调整占空比
    │   └─ adjust_duty()
    │       ├─ s_duty_index = (s_duty_index + 1) % DUTY_TABLE_SIZE
    │       ├─ motor_ctrl_set_duty(get_current_duty())
    │       │   └─ bsp_pwm_set_duty_cycle()
    │       └─ BSP_UART_Printf("Duty=...")
    │
    └─ KEY2长按：调整换相偏移
        └─ adjust_offset()
            ├─ 保存运行状态
            ├─ if (was_running) stop_motor()
            ├─ motor_phase_tab_set_commutation_offset(new_offset)
            ├─ BSP_UART_Printf("Offset=...")
            └─ if (was_running) start_motor()
```

### 3.2 按键处理代码详解

```c
static void handle_keys(void)
{
    /* 1. 获取按键事件 */
    KeyEvent_TypeDef key0 = Key_GetEvent(&s_key, 0U);
    KeyEvent_TypeDef key1 = Key_GetEvent(&s_key, 1U);
    KeyEvent_TypeDef key2 = Key_GetEvent(&s_key, 2U);

    /* 2. KEY0: 启动/停止 */
    if (key0 == KEY_EVENT_SHORT_PRESS) {
        if (motor_ctrl_is_running()) {
            stop_motor();            // 停止电机
        } else {
            start_motor();           // 启动电机
        }
    }

    /* 3. KEY1: 切换方向 */
    if (key1 == KEY_EVENT_SHORT_PRESS) {
        toggle_direction();          // 切换CW/CCW
    }

    /* 4. KEY2短按: 调整占空比 */
    if (key2 == KEY_EVENT_SHORT_PRESS) {
        adjust_duty();               // 8%→10%→15%→...→80%
    }

    /* 5. KEY2长按: 调整换相偏移 */
    if (key2 == KEY_EVENT_LONG_PRESS) {
        adjust_offset();             // 0→1→2→3→4→5→0
    }
}
```

---

## 4. 电机启动逻辑

### 4.1 启动流程图

```
start_motor()
    ↓
get_current_duty()
    ↓ 返回PWM计数值
motor_start_simple(duty, direction)
    ↓
bsp_ctrl_sd_enable()                 // 使能驱动器
    ↓
HAL_Delay(10ms)                      // 等待驱动器稳定
    ↓
bsp_hall_read_state()                // 读取当前霍尔状态
    ↓
motor_phase_tab_get_phase(state, dir, offset)
    ↓ 查表获取换相动作
bsp_pwm_set_phase(phase)             // 设置PWM相位
    ↓
bsp_pwm_set_duty_cycle(duty)         // 设置占空比
    ↓
更新电机状态为RUNNING
    ↓
bsp_hall_clear_stats()               // 清除统计数据
```

### 4.2 启动代码详解

```c
static void start_motor(void)
{
    /* 1. 获取当前占空比 */
    uint16_t duty = get_current_duty();
    // duty = (BSP_PWM_PERIOD + 1) * DUTY_PERCENT_TABLE[s_duty_index] / 100

    /* 2. 调用电机启动函数 */
    if (motor_start_simple(duty, motor_ctrl_get_direction()) == 0) {
        // 启动成功
        BSP_UART_Printf("[P5] Motor started, duty=%u%%\r\n",
                        DUTY_PERCENT_TABLE[s_duty_index]);
    } else {
        // 启动失败
        BSP_UART_Printf("[P5] Motor start failed\r\n");
    }
}
```

**motor_start_simple()内部逻辑**（在motor_ctrl.c中）：
```c
int motor_start_simple(uint16_t duty, uint8_t direction)
{
    /* 1. 使能驱动器 */
    bsp_ctrl_sd_enable();
    HAL_Delay(10);                   // 等待驱动器稳定

    /* 2. 读取霍尔状态 */
    uint8_t hall_state = bsp_hall_read_state();

    /* 3. 查表获取换相动作 */
    motor_phase_action_t phase = motor_phase_tab_get_phase(
        hall_state, direction, offset);

    /* 4. 设置PWM相位 */
    bsp_pwm_set_phase(phase);

    /* 5. 设置占空比 */
    bsp_pwm_set_duty_cycle(duty);

    /* 6. 更新状态 */
    motor_state = RUNNING;
    bsp_hall_clear_stats();

    return 0;  // 成功
}
```

---

## 5. 电机运行时的换相逻辑

### 5.1 换相流程图

```
主循环（后台）
    ↓
bsp_hall_poll_and_commutate()        // 在motor_ctrl.c中调用
    ↓
bsp_hall_read_state()                // 读取当前霍尔状态
    ↓
检测状态变化？
    ├─ 否 → 返回
    └─ 是 ↓
motor_phase_tab_is_valid_transition(old, new)
    ↓
有效转换？
    ├─ 否 → s_invalid_transition_count++ → 返回
    └─ 是 ↓
s_valid_transition_count++
    ↓
计算换相周期
    ↓
更新RPM滤波缓冲区
    ↓
motor_phase_tab_get_phase(state, dir, offset)
    ↓
bsp_pwm_set_phase(phase)             // 执行换相
    ↓
返回
```

### 5.2 换相代码详解

**在主循环中调用**（motor_ctrl.c）：
```c
// 在Test_Phase5_Loop()中，实际上motor_ctrl内部会调用
void motor_sensor_mode_phase(void)
{
    if (motor_state == RUNNING) {
        bsp_hall_poll_and_commutate();  // 轮询霍尔并换相
    }
}
```

**换相实现**（bsp_hall.c）：
```c
void bsp_hall_poll_and_commutate(void)
{
    /* 1. 读取霍尔状态 */
    uint8_t new_state = bsp_hall_read_state();

    /* 2. 检测状态变化 */
    if (new_state == s_last_hall_state) {
        return;  // 状态未变化
    }

    /* 3. 验证转换有效性 */
    if (!motor_phase_tab_is_valid_transition(s_last_hall_state, new_state)) {
        s_invalid_transition_count++;
        return;  // 无效转换，不换相
    }

    /* 4. 有效转换处理 */
    s_valid_transition_count++;

    /* 5. 计算换相周期 */
    uint32_t period = TIM5->CNT - s_last_tim5_cnt;
    s_last_tim5_cnt = TIM5->CNT;

    /* 6. 更新RPM滤波缓冲区 */
    s_period_buffer[s_period_buffer_index] = period;
    s_period_buffer_index = (s_period_buffer_index + 1) % 6;

    /* 7. 获取新相位 */
    motor_phase_action_t phase = motor_phase_tab_get_phase(
        new_state, motor_ctrl_get_direction(), offset);

    /* 8. 执行换相 */
    bsp_pwm_set_phase(phase);

    /* 9. 更新状态 */
    s_last_hall_state = new_state;
}
```

---

## 6. 故障处理逻辑

### 6.1 故障处理流程图

```
handle_faults()
    ├─ 检查硬件故障（过流）
    │   └─ if (bsp_pwm_is_break_fault())
    │       ├─ motor_ctrl_set_fault(MOTOR_FAULT_BREAK)
    │       ├─ stop_motor()
    │       ├─ BSP_UART_Printf("Break fault!")
    │       └─ bsp_pwm_clear_break_fault()
    │
    └─ 检查霍尔无效故障
        └─ if (motor_ctrl_is_running() &&
               motor_ctrl_get_fault() == MOTOR_FAULT_HALL_INVALID)
            ├─ stop_motor()
            └─ BSP_UART_Printf("Hall invalid!")
```

### 6.2 故障处理代码详解

```c
static void handle_faults(void)
{
    /* 1. 硬件故障（过流等） */
    if (bsp_pwm_is_break_fault()) {
        motor_ctrl_set_fault(MOTOR_FAULT_BREAK);
        stop_motor();
        BSP_UART_Printf("[P5] Break fault!\r\n");
        bsp_pwm_clear_break_fault();
    }

    /* 2. 霍尔无效故障 */
    if (motor_ctrl_is_running() &&
        (motor_ctrl_get_fault() == MOTOR_FAULT_HALL_INVALID)) {
        stop_motor();
        BSP_UART_Printf("[P5] Hall invalid!\r\n");
    }
}
```

---

## 7. 状态打印逻辑

### 7.1 打印流程图

```
print_status()
    ↓
bsp_hall_get_state()                 // 读取霍尔状态
    ↓
format_hall_state(state, buf)        // 格式化为"110"
    ↓
bsp_adc_get_vbus()                   // 读取母线电压
    ↓
BSP_UART_Printf(...)                 // 打印状态
    ├─ Run: 运行状态
    ├─ Dir: 方向
    ├─ Hall: 霍尔状态
    ├─ Duty: 占空比
    ├─ RPM: 转速
    └─ VBUS: 母线电压
```

### 7.2 打印代码详解

```c
static void print_status(void)
{
    /* 1. 读取霍尔状态 */
    uint8_t hall_state = bsp_hall_get_state();
    char hall_str[4];
    format_hall_state(hall_state, hall_str);  // "110"

    /* 2. 读取母线电压 */
    uint32_t vbus_mv = (uint32_t)(bsp_adc_get_vbus() * 1000.0f);

    /* 3. 打印状态（简化版） */
    BSP_UART_Printf("[P5] Run=%u Dir=%s Hall=%s Duty=%u%% RPM=%lu VBUS=%lu.%03luV\r\n",
                    motor_ctrl_is_running(),
                    (motor_ctrl_get_direction() == MOTOR_DIR_CCW) ? "CCW" : "CW",
                    hall_str,
                    DUTY_PERCENT_TABLE[s_duty_index],
                    bsp_hall_get_speed(),
                    vbus_mv / 1000U,
                    vbus_mv % 1000U);
}
```

---

## 8. 数据流图

### 8.1 完整数据流

```
硬件层
    ↓
霍尔传感器(GPIO) → bsp_hall_read_state() → 霍尔状态(0-7)
    ↓
换相表查询 → motor_phase_tab_get_phase() → PWM相位
    ↓
PWM输出 → bsp_pwm_set_phase() → 三相桥驱动
    ↓
电机旋转 → 霍尔状态变化 → 循环

ADC采样(DMA) → bsp_adc_get_vbus() → 母线电压
    ↓
串口打印 → BSP_UART_Printf() → 上位机显示

按键输入(GPIO) → Key_Scan() → 按键事件
    ↓
事件处理 → handle_keys() → 电机控制
```

---

## 9. 时序图

### 9.1 主循环时序

```
时间轴 →
0ms    200ms   400ms   600ms   800ms   1000ms
│      │       │       │       │       │
├─LED──┼───────┼───────┼───────┼───────┤  200ms周期
│      │       │       │       │       │
├KEY┬KEY┬KEY┬KEY┬KEY┬KEY┬KEY┬KEY┬KEY┬KEY  20ms周期
│   │   │   │   │   │   │   │   │   │
├───┴───┴───┴───┴───┴───┴───┴───┴───┴──  故障检查（实时）
│                       │               │
└───────────────────────┴───────────────┘  800ms周期（打印）
```

---

## 10. 调用关系总结

### 10.1 模块依赖关系

```
test_phase5.c
    ├─ 依赖 BSP层
    │   ├─ bsp_uart.c      (串口通信)
    │   ├─ bsp_hall.c      (霍尔传感器)
    │   ├─ bsp_pwm.c       (PWM输出)
    │   ├─ bsp_adc_motor.c (ADC采样)
    │   ├─ bsp_key.c       (按键扫描)
    │   ├─ bsp_led.c       (LED控制)
    │   └─ bsp_ctrl_sd.c   (驱动器使能)
    │
    └─ 依赖 Motor层
        ├─ motor_ctrl.c    (电机控制)
        └─ motor_phase_tab.c (换相表)
```

### 10.2 关键函数调用频率

| 函数 | 调用频率 | 说明 |
|------|---------|------|
| Test_Phase5_Loop() | ~1kHz | 主循环 |
| update_led() | 5Hz | LED更新 |
| Key_Scan() | 50Hz | 按键扫描 |
| handle_keys() | 50Hz | 按键处理 |
| handle_faults() | ~1kHz | 故障检查 |
| print_status() | 1.25Hz | 状态打印 |
| bsp_hall_poll_and_commutate() | ~1kHz | 换相检测 |

---

## 11. 关键变量说明

| 变量 | 类型 | 作用 |
|------|------|------|
| s_duty_index | uint8_t | 当前占空比索引（0-15） |
| s_init_ok | uint8_t | 初始化完成标志 |
| s_led_tick | uint32_t | LED更新时间戳 |
| s_key_tick | uint32_t | 按键扫描时间戳 |
| s_print_tick | uint32_t | 状态打印时间戳 |
| s_key | KeyGroup_HandleTypeDef | 按键组句柄 |
| DUTY_PERCENT_TABLE[] | const uint8_t[] | 占空比百分比表 |

---

## 12. 总结

test_phase5.c的调用逻辑清晰简洁：

1. **初始化阶段**：按顺序初始化所有外设和模块
2. **主循环阶段**：基于时间片轮询各个任务
3. **按键处理**：事件驱动，响应用户操作
4. **电机控制**：通过BSP和Motor层实现
5. **故障处理**：实时检测，立即响应
6. **状态监控**：定期打印，便于调试

整体架构采用**轮询+时间片**的设计，简单可靠，易于理解和维护。
