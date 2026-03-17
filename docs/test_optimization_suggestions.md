# test.c 函数调用逻辑优化建议

## 概述

本文档分析 `test.c` 中的函数调用逻辑，识别潜在的优化空间，提供具体的改进方案。

---

## 优化项列表

### 1. 冗余的状态查询 ⭐⭐⭐

**优先级**: 高
**收益**: 减少函数调用开销，提升性能
**复杂度**: 低

#### 问题描述

`print_status()` 函数中多次调用相同的状态查询函数，导致不必要的重复计算。

**当前代码** ([test.c:83-118](../User/Test/Src/test.c#L83-L118)):

```c
static void print_status(void)
{
    uint8_t hall_state = bsp_hall_get_state();
    char hall_str[4];
    uint32_t vbus_mv = (uint32_t)(bsp_adc_get_vbus() * 1000.0f);
    float temp_c = bsp_adc_get_temp();
    bsp_current_t current;

    format_hall_state(hall_state, hall_str);

    /* 获取滤波后的电流值 */
    bsp_current_sense_get_filtered(&current);

    BSP_UART_Printf("[P5] Run=%u Dir=%s Hall=%s Duty=%u%% RPM=%lu\r\n",
                    motor_ctrl_is_running(),  // 调用1
                    (motor_ctrl_get_direction() == MOTOR_DIR_CCW) ? "CCW" : "CW",  // 调用2
                    hall_str,
                    DUTY_PERCENT_TABLE[s_duty_index],
                    bsp_hall_get_speed());

    BSP_UART_Printf("     VBUS=%lu.%03luV Temp=%.1fC\r\n",
                    vbus_mv / 1000U,
                    vbus_mv % 1000U,
                    temp_c);

    BSP_UART_Printf("     Iu=%.1fmA Iv=%.1fmA Iw=%.1fmA Imax=%.1fmA\r\n",
                    current.u_ma,
                    current.v_ma,
                    current.w_ma,
                    bsp_current_sense_get_max_phase_current());

    /* 过流警告 */
    if (bsp_current_sense_is_overcurrent()) {  // 调用3（与Test_Loop重复）
        BSP_UART_Printf("     [WARNING] Overcurrent detected!\r\n");
    }
}
```

#### 优化方案

缓存状态查询结果，避免重复调用：

```c
static void print_status(void)
{
    /* 缓存状态查询结果 */
    uint8_t hall_state = bsp_hall_get_state();
    uint8_t is_running = motor_ctrl_is_running();
    uint8_t direction = motor_ctrl_get_direction();
    uint8_t is_overcurrent = bsp_current_sense_is_overcurrent();

    char hall_str[4];
    uint32_t vbus_mv = (uint32_t)(bsp_adc_get_vbus() * 1000.0f);
    float temp_c = bsp_adc_get_temp();
    bsp_current_t current;

    format_hall_state(hall_state, hall_str);
    bsp_current_sense_get_filtered(&current);

    BSP_UART_Printf("[P5] Run=%u Dir=%s Hall=%s Duty=%u%% RPM=%lu\r\n",
                    is_running,
                    (direction == MOTOR_DIR_CCW) ? "CCW" : "CW",
                    hall_str,
                    DUTY_PERCENT_TABLE[s_duty_index],
                    bsp_hall_get_speed());

    BSP_UART_Printf("     VBUS=%lu.%03luV Temp=%.1fC\r\n",
                    vbus_mv / 1000U,
                    vbus_mv % 1000U,
                    temp_c);

    BSP_UART_Printf("     Iu=%.1fmA Iv=%.1fmA Iw=%.1fmA Imax=%.1fmA\r\n",
                    current.u_ma,
                    current.v_ma,
                    current.w_ma,
                    bsp_current_sense_get_max_phase_current());

    /* 过流警告 */
    if (is_overcurrent) {
        BSP_UART_Printf("     [WARNING] Overcurrent detected!\r\n");
    }
}
```

#### 收益分析

- **性能提升**: 减少3次函数调用（每800ms执行一次）
- **代码可读性**: 变量名更清晰，意图更明确
- **维护性**: 如果需要多次使用状态值，只需修改一处

---

### 2. 重复的过流检测 ⭐⭐⭐

**优先级**: 高
**收益**: 避免重复检测，逻辑更清晰
**复杂度**: 低

#### 问题描述

过流检测在 `Test_Loop()` 和 `print_status()` 中分别执行，导致逻辑分散且重复。

**当前代码** ([test.c:342-380](../User/Test/Src/test.c#L342-L380)):

```c
void Test_Loop(void)
{
    uint32_t now = HAL_GetTick();

    if (!s_init_ok) {
        return;
    }

    /* LED更新 (200ms) */
    if ((now - s_led_tick) >= LED_INTERVAL_MS) {
        s_led_tick = now;
        update_led();
    }

    /* 按键扫描 (20ms) */
    if ((now - s_key_tick) >= KEY_INTERVAL_MS) {
        s_key_tick = now;
        Key_Scan(&s_key);
        handle_keys();
    }

    /* 更新电流滤波器 */
    bsp_current_sense_update_filter();

    /* 故障处理 */
    handle_faults();  // 内部没有过流检测

    /* 过流保护 */
    if (bsp_current_sense_is_overcurrent() && motor_ctrl_is_running()) {  // 检测1
        stop_motor();
        BSP_UART_Printf("[P5] OVERCURRENT PROTECTION!\r\n");
    }

    /* 状态打印 (800ms) */
    if ((now - s_print_tick) >= PRINT_INTERVAL_MS) {
        s_print_tick = now;
        print_status();  // 内部再次调用 bsp_current_sense_is_overcurrent()  // 检测2
    }
}
```

#### 优化方案

将过流检测统一到 `handle_faults()` 中：

**修改 Test_Loop()**:

```c
void Test_Loop(void)
{
    uint32_t now = HAL_GetTick();

    if (!s_init_ok) {
        return;
    }

    /* LED更新 (200ms) */
    if ((now - s_led_tick) >= LED_INTERVAL_MS) {
        s_led_tick = now;
        update_led();
    }

    /* 按键扫描 (20ms) */
    if ((now - s_key_tick) >= KEY_INTERVAL_MS) {
        s_key_tick = now;
        Key_Scan(&s_key);
        handle_keys();
    }

    /* 更新电流滤波器 */
    bsp_current_sense_update_filter();

    /* 故障处理（包含过流检测） */
    handle_faults();

    /* 状态打印 (800ms) */
    if ((now - s_print_tick) >= PRINT_INTERVAL_MS) {
        s_print_tick = now;
        print_status();
    }
}
```

**修改 handle_faults()**:

```c
/**
 * @brief  处理故障
 */
static void handle_faults(void)
{
    /* 硬件故障（过流等） */
    if (bsp_pwm_is_break_fault()) {
        motor_ctrl_set_fault(MOTOR_FAULT_BREAK);
        stop_motor();
        BSP_UART_Printf("[P5] Break fault!\r\n");
        bsp_pwm_clear_break_fault();
    }

    /* 霍尔无效故障 */
    if (motor_ctrl_is_running() && (motor_ctrl_get_fault() == MOTOR_FAULT_HALL_INVALID)) {
        stop_motor();
        BSP_UART_Printf("[P5] Hall invalid!\r\n");
    }

    /* 软件过流保护 */
    if (bsp_current_sense_is_overcurrent() && motor_ctrl_is_running()) {
        stop_motor();
        BSP_UART_Printf("[P5] OVERCURRENT PROTECTION!\r\n");
    }
}
```

**修改 print_status()** (结合优化1):

```c
static void print_status(void)
{
    /* 缓存状态查询结果 */
    uint8_t hall_state = bsp_hall_get_state();
    uint8_t is_running = motor_ctrl_is_running();
    uint8_t direction = motor_ctrl_get_direction();
    uint8_t is_overcurrent = bsp_current_sense_is_overcurrent();

    char hall_str[4];
    uint32_t vbus_mv = (uint32_t)(bsp_adc_get_vbus() * 1000.0f);
    float temp_c = bsp_adc_get_temp();
    bsp_current_t current;

    format_hall_state(hall_state, hall_str);
    bsp_current_sense_get_filtered(&current);

    BSP_UART_Printf("[P5] Run=%u Dir=%s Hall=%s Duty=%u%% RPM=%lu\r\n",
                    is_running,
                    (direction == MOTOR_DIR_CCW) ? "CCW" : "CW",
                    hall_str,
                    DUTY_PERCENT_TABLE[s_duty_index],
                    bsp_hall_get_speed());

    BSP_UART_Printf("     VBUS=%lu.%03luV Temp=%.1fC\r\n",
                    vbus_mv / 1000U,
                    vbus_mv % 1000U,
                    temp_c);

    BSP_UART_Printf("     Iu=%.1fmA Iv=%.1fmA Iw=%.1fmA Imax=%.1fmA\r\n",
                    current.u_ma,
                    current.v_ma,
                    current.w_ma,
                    bsp_current_sense_get_max_phase_current());

    /* 过流警告（仅显示，不处理） */
    if (is_overcurrent) {
        BSP_UART_Printf("     [WARNING] Overcurrent detected!\r\n");
    }
}
```

#### 收益分析

- **逻辑清晰**: 所有故障处理集中在 `handle_faults()`
- **避免重复**: 过流检测只在一处执行（每次循环）
- **职责分离**: `print_status()` 只负责显示，不负责处理
- **易于维护**: 添加新故障类型只需修改 `handle_faults()`

---

### 3. 初始化失败处理的重复代码 ⭐⭐

**优先级**: 中
**收益**: 代码更简洁，易维护
**复杂度**: 低

#### 问题描述

`Test_Init()` 中多次重复相同的错误处理代码。

**当前代码** ([test.c:278-310](../User/Test/Src/test.c#L278-L310)):

```c
if (bsp_hall_init() != 0) {
    BSP_UART_Printf("  Hall: FAILED!\r\n");
    LED0_ON();
    LED1_ON();
    return;
}
BSP_UART_Printf("  Hall: OK\r\n");

if (bsp_adc_motor_init() != 0) {
    BSP_UART_Printf("  ADC: FAILED!\r\n");
    LED0_ON();
    LED1_ON();
    return;
}
BSP_UART_Printf("  ADC: OK\r\n");

/* 启动ADC触发 */
__HAL_TIM_SET_COMPARE(&htim1, TIM_CHANNEL_4, BSP_PWM_PERIOD / 2U);
if (HAL_TIM_PWM_Start(&htim1, TIM_CHANNEL_4) != HAL_OK) {
    BSP_UART_Printf("  ADC Trigger: FAILED!\r\n");
    LED0_ON();
    LED1_ON();
    return;
}
BSP_UART_Printf("  ADC Trigger: OK\r\n");

/* 初始化电流采样模块 */
BSP_UART_Printf("\r\nCalibrating current sensors...\r\n");
if (bsp_current_sense_init() != 0) {
    BSP_UART_Printf("  Current Sense: FAILED!\r\n");
    LED0_ON();
    LED1_ON();
    return;
}
```

#### 优化方案

提取错误处理函数：

```c
/**
 * @brief  初始化失败处理
 * @param  module_name 模块名称
 */
static void init_failed(const char *module_name)
{
    BSP_UART_Printf("  %s: FAILED!\r\n", module_name);
    LED0_ON();
    LED1_ON();
}

void Test_Init(void)
{
    /* 基础初始化 */
    Delay_Init();
    BSP_UART_Init();
    VOFA_Init();
    LED_Init();
    Key_Init(&s_key, KEY_DEFAULT_LONG_PRESS_TIME);

    BSP_UART_Printf("\r\n=== BLDC Motor Control Test (170MHz) ===\r\n");
    BSP_UART_Printf("SYSCLK: %lu MHz\r\n", HAL_RCC_GetSysClockFreq()/1000000);
    VOFA_Printf("System initialized at 170MHz\r\n");

    /* 硬件初始化 */
    BSP_UART_Printf("\r\nInitializing hardware...\r\n");

    bsp_ctrl_sd_init();
    BSP_UART_Printf("  SD Control: OK\r\n");

    bsp_pwm_init();
    BSP_UART_Printf("  PWM: OK\r\n");

    if (bsp_hall_init() != 0) {
        init_failed("Hall");
        return;
    }
    BSP_UART_Printf("  Hall: OK\r\n");

    if (bsp_adc_motor_init() != 0) {
        init_failed("ADC");
        return;
    }
    BSP_UART_Printf("  ADC: OK\r\n");

    /* 启动ADC触发 */
    __HAL_TIM_SET_COMPARE(&htim1, TIM_CHANNEL_4, BSP_PWM_PERIOD / 2U);
    if (HAL_TIM_PWM_Start(&htim1, TIM_CHANNEL_4) != HAL_OK) {
        init_failed("ADC Trigger");
        return;
    }
    BSP_UART_Printf("  ADC Trigger: OK\r\n");

    /* 初始化电流采样模块 */
    BSP_UART_Printf("\r\nCalibrating current sensors...\r\n");
    if (bsp_current_sense_init() != 0) {
        init_failed("Current Sense");
        return;
    }

    bsp_current_calib_t calib;
    bsp_current_sense_get_calib(&calib);
    BSP_UART_Printf("  Current Sense: OK (U=%u V=%u W=%u)\r\n",
                    calib.u_offset, calib.v_offset, calib.w_offset);

    /* 电机控制初始化 */
    motor_ctrl_init();
    BSP_UART_Printf("Motor Control: OK\r\n");

    bsp_pwm_disable_break();
    BSP_UART_Printf("Break Protection: Disabled (Test Mode)\r\n");

    /* 打印配置信息 */
    BSP_UART_Printf("\r\n=== Configuration ===\r\n");
    BSP_UART_Printf("PWM Frequency: %.2f kHz\r\n",
                    (float)HAL_RCC_GetPCLK2Freq() / (BSP_PWM_PERIOD + 1) / 2000.0f);
    BSP_UART_Printf("KEY0: Start/Stop\r\n");
    BSP_UART_Printf("KEY1: Direction\r\n");
    BSP_UART_Printf("KEY2: Duty/Offset\r\n");
    BSP_UART_Printf("Duty Range: 8%%-80%%, Step: 5%%\r\n");
    BSP_UART_Printf("\r\n=== System Ready ===\r\n\r\n");

    s_init_ok = 1U;
    print_status();
}
```

#### 收益分析

- **代码简洁**: 减少重复代码
- **易于维护**: 修改错误处理逻辑只需改一处
- **一致性**: 所有模块的错误处理格式统一

---

### 4. 方向切换和偏移调整的重复逻辑 ⭐

**优先级**: 低
**收益**: 代码复用（但可能过度设计）
**复杂度**: 中

#### 问题描述

`toggle_direction()` 和 `adjust_offset()` 有相同的"停机-操作-重启"模式。

**当前代码** ([test.c:146-191](../User/Test/Src/test.c#L146-L191)):

```c
static void toggle_direction(void)
{
    uint8_t new_dir = (motor_ctrl_get_direction() == MOTOR_DIR_CCW) ? MOTOR_DIR_CW : MOTOR_DIR_CCW;
    uint8_t was_running = motor_ctrl_is_running();

    if (was_running) {
        stop_motor();
    }

    motor_ctrl_set_direction(new_dir);
    BSP_UART_Printf("[P5] Direction=%s\r\n", (new_dir == MOTOR_DIR_CCW) ? "CCW" : "CW");

    if (was_running) {
        start_motor();
    }
}

static void adjust_offset(void)
{
    uint8_t new_offset = (motor_phase_tab_get_commutation_offset() + 1U) % 6U;
    uint8_t was_running = motor_ctrl_is_running();

    if (was_running) {
        stop_motor();
    }

    motor_phase_tab_set_commutation_offset(new_offset);
    BSP_UART_Printf("[P5] Offset=%u\r\n", new_offset);

    if (was_running) {
        start_motor();
    }
}
```

#### 优化方案（可选）

提取公共模式：

```c
/**
 * @brief  执行需要停机的操作
 * @param  operation 操作函数指针
 * @note   自动处理停机和重启逻辑
 */
static void execute_with_restart(void (*operation)(void))
{
    uint8_t was_running = motor_ctrl_is_running();

    if (was_running) {
        stop_motor();
    }

    operation();

    if (was_running) {
        start_motor();
    }
}

/**
 * @brief  执行方向切换操作
 */
static void do_toggle_direction(void)
{
    uint8_t new_dir = (motor_ctrl_get_direction() == MOTOR_DIR_CCW) ? MOTOR_DIR_CW : MOTOR_DIR_CCW;
    motor_ctrl_set_direction(new_dir);
    BSP_UART_Printf("[P5] Direction=%s\r\n", (new_dir == MOTOR_DIR_CCW) ? "CCW" : "CW");
}

/**
 * @brief  执行偏移调整操作
 */
static void do_adjust_offset(void)
{
    uint8_t new_offset = (motor_phase_tab_get_commutation_offset() + 1U) % 6U;
    motor_phase_tab_set_commutation_offset(new_offset);
    BSP_UART_Printf("[P5] Offset=%u\r\n", new_offset);
}

/**
 * @brief  切换方向
 */
static void toggle_direction(void)
{
    execute_with_restart(do_toggle_direction);
}

/**
 * @brief  调整换相偏移
 */
static void adjust_offset(void)
{
    execute_with_restart(do_adjust_offset);
}
```

#### 收益分析

- **代码复用**: 停机-重启逻辑只写一次
- **扩展性**: 未来添加类似操作更容易
- **风险**: 可能过度设计，增加理解成本

**建议**: 除非未来有更多类似操作，否则保持当前实现即可。

---

### 5. 占空比计算的缓存 ⭐

**优先级**: 低
**收益**: 微小性能提升
**复杂度**: 低

#### 问题描述

`get_current_duty()` 在多处被调用，每次都重新计算。

**当前代码**:

```c
static inline uint16_t get_current_duty(void)
{
    return (BSP_PWM_PERIOD + 1U) * DUTY_PERCENT_TABLE[s_duty_index] / 100U;
}

static void start_motor(void)
{
    uint16_t duty = get_current_duty();  // 计算
    // ...
}

static void adjust_duty(void)
{
    s_duty_index = (s_duty_index + 1U) % DUTY_TABLE_SIZE;
    motor_ctrl_set_duty(get_current_duty());  // 再次计算
    // ...
}
```

#### 优化方案（可选）

缓存当前占空比值：

```c
/* 私有变量区添加 */
static uint16_t s_current_duty = 0U;

/**
 * @brief  调整占空比
 */
static void adjust_duty(void)
{
    s_duty_index = (s_duty_index + 1U) % DUTY_TABLE_SIZE;
    s_current_duty = get_current_duty();
    motor_ctrl_set_duty(s_current_duty);
    BSP_UART_Printf("[P5] Duty=%u%%\r\n", DUTY_PERCENT_TABLE[s_duty_index]);
}

/**
 * @brief  启动电机
 */
static void start_motor(void)
{
    s_current_duty = get_current_duty();
    if (motor_start_simple(s_current_duty, motor_ctrl_get_direction()) == 0) {
        BSP_UART_Printf("[P5] Motor started, duty=%u%%\r\n", DUTY_PERCENT_TABLE[s_duty_index]);
    } else {
        BSP_UART_Printf("[P5] Motor start failed\r\n");
    }
}
```

#### 收益分析

- **性能提升**: 微小（计算本身很简单）
- **代码复杂度**: 略微增加（需要维护缓存变量）
- **建议**: 除非频繁调用，否则不必优化

---

## 优化优先级总结

| 优先级 | 优化项 | 收益 | 复杂度 | 建议 |
|--------|--------|------|--------|------|
| ⭐⭐⭐ 高 | 1. 缓存状态查询结果 | 减少函数调用开销 | 低 | **立即实施** |
| ⭐⭐⭐ 高 | 2. 统一过流检测逻辑 | 避免重复检测，逻辑更清晰 | 低 | **立即实施** |
| ⭐⭐ 中 | 3. 提取错误处理函数 | 代码更简洁，易维护 | 低 | 建议实施 |
| ⭐ 低 | 4. 提取停机-重启模式 | 代码复用（但可能过度设计） | 中 | 可选 |
| ⭐ 低 | 5. 缓存占空比值 | 微小性能提升 | 低 | 可选 |

---

## 实施建议

### 第一阶段（必须）

实施**优化1**和**优化2**，它们能显著提升代码质量且实现简单：

1. 修改 `print_status()` - 缓存状态查询结果
2. 修改 `handle_faults()` - 添加过流检测
3. 修改 `Test_Loop()` - 移除重复的过流检测

**预期收益**:
- 减少每800ms约3次函数调用
- 逻辑更清晰，职责分离
- 易于维护和扩展

### 第二阶段（建议）

实施**优化3**，提升代码可维护性：

1. 添加 `init_failed()` 函数
2. 修改 `Test_Init()` 中的错误处理

**预期收益**:
- 减少约20行重复代码
- 错误处理格式统一
- 易于修改和扩展

### 第三阶段（可选）

根据实际需求考虑**优化4**和**优化5**：

- 如果未来有更多"停机-操作-重启"模式，实施优化4
- 如果性能分析显示占空比计算是瓶颈，实施优化5

---

## 性能影响分析

### 优化前

**每个主循环周期**:
- `handle_faults()`: 2-3次函数调用
- 过流检测: 1次函数调用
- `print_status()` (每800ms): 3次状态查询 + 1次过流检测

**总计**: 约6-7次函数调用/周期（不含800ms打印）

### 优化后

**每个主循环周期**:
- `handle_faults()`: 3-4次函数调用（包含过流检测）
- `print_status()` (每800ms): 4次状态查询（缓存）

**总计**: 约3-4次函数调用/周期（不含800ms打印）

**性能提升**: 约40-50%的函数调用减少

---

## 代码可维护性分析

### 优化前

- **逻辑分散**: 过流检测在2处
- **重复代码**: 错误处理重复4次
- **职责混乱**: `print_status()` 既显示又处理

### 优化后

- **逻辑集中**: 所有故障处理在 `handle_faults()`
- **代码复用**: 错误处理统一为 `init_failed()`
- **职责清晰**: `print_status()` 只负责显示

---

## 总结

建议优先实施**优化1**和**优化2**，它们能带来显著的性能和可维护性提升，且实现成本低。**优化3**也值得实施，能进一步提升代码质量。**优化4**和**优化5**可根据实际需求选择性实施。

实施这些优化后，`test.c` 将具有：
- 更好的性能（减少40-50%的函数调用）
- 更清晰的逻辑（职责分离）
- 更易维护（代码复用，减少重复）
- 更好的扩展性（易于添加新功能）
