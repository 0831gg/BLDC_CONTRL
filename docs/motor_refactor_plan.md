# 电机控制系统三阶段改造方案

## 概述

本文档详细描述了将当前BLDC电机控制实现改造为参考实现（正点原子）风格的完整方案。改造分为三个阶段，每个阶段独立可测试。

**改造目标：**
1. 简化启动序列 - 去掉复杂的ALIGN/KICK阶段
2. PWM始终开启 - 提高时序稳定性
3. 定时器轮询 - 从EXTI事件驱动改为高频轮询

**推荐执行顺序：** 阶段1 → 阶段2 → 阶段3（逐步测试）

---

## 阶段1：简化启动序列

### 目标
去掉复杂的ALIGN/KICK序列，像参考实现一样直接根据当前霍尔状态开始换相。

### CubeMX配置
✅ 无需修改CubeMX配置

### 代码修改

#### 文件1：`User/Motor/Inc/motor_ctrl.h`

**位置：** 第26行后添加

```c
// 简化启动函数声明
int motor_start_simple(uint16_t duty, uint8_t direction);
```

#### 文件2：`User/Motor/Src/motor_ctrl.c`

**位置：** 在 `motor_start_assisted()` 函数后添加

```c
/**
 * @brief 简化启动 - 直接根据当前霍尔状态换相
 * @param duty 启动占空比
 * @param direction 旋转方向
 * @return 0=成功, -1=失败
 */
int motor_start_simple(uint16_t duty, uint8_t direction)
{
    // 1. 设置基本参数
    s_motor_fault = MOTOR_FAULT_NONE;
    s_motor_direction = direction;
    s_motor_duty = duty;

    // 2. 清除统计和故障
    bsp_hall_clear_stats();
    bsp_pwm_clear_break_fault();
    bsp_pwm_duty_set(s_motor_duty);

    // 3. 标记为运行状态
    s_motor_running = 1U;
    motor_ctrl_set_startup_trace(MOTOR_STARTUP_STAGE_TRACKING,
                                 0U,
                                 bsp_hall_get_state(),
                                 MOTOR_PHASE_ACTION_INVALID,
                                 s_motor_duty);

    // 4. 根据当前霍尔状态立即换相
    motor_sensor_mode_phase();

    // 5. 检查是否有故障
    if (s_motor_fault != MOTOR_FAULT_NONE) {
        motor_stop();
        return -1;
    }

    // 6. 使能驱动器和霍尔中断
    bsp_ctrl_sd_enable();
    bsp_hall_irq_enable();

    return 0;
}
```

#### 文件3：`User/Test/Src/test_phase5.c`

**位置：** 第124行左右，修改启动函数调用

```c
// 原来：
if (motor_start_assisted(s_duty_table[s_duty_index], motor_ctrl_get_direction()) == 0) {

// 改为：
if (motor_start_simple(s_duty_table[s_duty_index], motor_ctrl_get_direction()) == 0) {
```

### 预期效果
- ✅ 启动更快（无80ms对齐 + 6步踢启延迟）
- ✅ 启动更简单直接
- ⚠️ 可能需要更高的初始占空比（建议25-30%）

### 测试方法
1. 编译烧录
2. 按KEY0启动电机
3. 观察是否能直接启动旋转
4. 如果不转，尝试增加占空比到30-35%

### 回退方案
如果测试失败，将 `test_phase5.c` 中的函数调用改回 `motor_start_assisted()`

---

## 阶段2：PWM始终开启

### 目标
让PWM通道从初始化开始就持续运行，通过SD引脚和下桥臂GPIO控制电机，而不是动态启停PWM。

### CubeMX配置
✅ 无需修改CubeMX配置

### 代码修改

#### 文件1：`User/Bsp/Src/bsp_pwm.c`

**修改1：** `bsp_pwm_init()` 函数

```c
void bsp_pwm_init(void)
{
    s_break_fault = 0U;
    s_pwm_duty = 0U;

    bsp_pwm_duty_set(0U);
    bsp_pwm_lower_all_off();

    // ✨ 新增：立即启动所有PWM通道
    HAL_TIM_PWM_Start(&htim1, TIM_CHANNEL_1);
    HAL_TIM_PWM_Start(&htim1, TIM_CHANNEL_2);
    HAL_TIM_PWM_Start(&htim1, TIM_CHANNEL_3);
    __HAL_TIM_MOE_ENABLE(&htim1);
}
```

**修改2：** `bsp_pwm_all_close()` 函数

```c
void bsp_pwm_all_close(void)
{
    // 不再停止PWM，只禁用通道输出
    bsp_pwm_disable_all_channels();

    // ❌ 删除以下三行：
    // HAL_TIM_PWM_Stop(&htim1, TIM_CHANNEL_1);
    // HAL_TIM_PWM_Stop(&htim1, TIM_CHANNEL_2);
    // HAL_TIM_PWM_Stop(&htim1, TIM_CHANNEL_3);
}
```

**修改3：** `bsp_pwm_all_open()` 函数

```c
void bsp_pwm_all_open(void)
{
    // ❌ 删除以下三行（PWM已经在运行）：
    // HAL_TIM_PWM_Start(&htim1, TIM_CHANNEL_1);
    // HAL_TIM_PWM_Start(&htim1, TIM_CHANNEL_2);
    // HAL_TIM_PWM_Start(&htim1, TIM_CHANNEL_3);

    // 保留MOE使能（虽然可能已经使能）
    __HAL_TIM_MOE_ENABLE(&htim1);
}
```

#### 文件2：`User/Motor/Src/motor_ctrl.c`

**修改1：** `motor_ctrl_prepare_start()` 函数

```c
static void motor_ctrl_prepare_start(uint16_t duty, uint8_t direction)
{
    s_motor_fault = MOTOR_FAULT_NONE;
    s_motor_direction = direction;
    s_motor_duty = duty;

    bsp_hall_clear_stats();
    bsp_hall_irq_disable();
    bsp_ctrl_sd_disable();
    bsp_pwm_clear_break_fault();
    bsp_pwm_lower_all_off();
    // ❌ 删除：bsp_pwm_all_close();  // PWM保持运行
    bsp_pwm_duty_set(s_motor_duty);
    // ❌ 删除：bsp_pwm_all_open();   // PWM已经在运行
    s_motor_running = 1U;
    motor_ctrl_set_startup_trace(MOTOR_STARTUP_STAGE_PREPARE,
                                 0U,
                                 bsp_hall_get_state(),
                                 MOTOR_PHASE_ACTION_INVALID,
                                 s_motor_duty);
}
```

**修改2：** `motor_stop()` 函数

```c
void motor_stop(void)
{
    bsp_ctrl_sd_disable();
    bsp_pwm_lower_all_off();
    bsp_pwm_all_close();  // 只禁用通道，不停止PWM
    s_motor_running = 0U;
    s_last_action = MOTOR_PHASE_ACTION_INVALID;
    bsp_hall_irq_enable();
}
```

### 预期效果
- ✅ PWM定时器持续运行，时序更稳定
- ✅ 换相时只需切换通道使能和下桥臂GPIO
- ✅ 减少启动/停止时的瞬态干扰

### 测试方法
1. 编译烧录
2. 用示波器观察PA8/PA9/PA10，应该始终有PWM波形（占空比可能为0）
3. 测试启动/停止是否正常

### 回退方案
恢复 `bsp_pwm_all_close()` 和 `bsp_pwm_all_open()` 中的启停代码

---

## 阶段3：改为定时器轮询（最大改动）

### 目标
从EXTI事件驱动改为TIM1更新中断中的高频轮询，像参考实现一样。

### CubeMX配置修改

#### 步骤1：禁用霍尔传感器EXTI中断

1. 打开CubeMX项目文件
2. 找到 **PA0/PA1/PA2**（HALL_U/V/W）引脚配置
3. **GPIO模式** 从 `External Interrupt Mode` 改为 `Input mode`
4. 取消勾选 `GPIO Pull-up`（如果有外部上拉）
5. 保存配置

#### 步骤2：使能TIM1更新中断

1. 在 `Timers` → `TIM1` → `NVIC Settings`
2. ✅ 勾选 `TIM1 update interrupt`
3. 设置优先级（建议 **Preemption Priority = 1**）
4. 保存配置

#### 步骤3：重新生成代码

1. 点击 `GENERATE CODE`
2. 等待代码生成完成

### 代码修改

#### 文件1：`User/Bsp/Inc/bsp_hall.h`

**位置：** 在现有函数声明后添加

```c
/**
 * @brief 定时器轮询霍尔传感器并换相
 * @note 在TIM1更新中断中调用
 */
void bsp_hall_poll_and_commutate(void);
```

#### 文件2：`User/Bsp/Src/bsp_hall.c`

**修改1：** `bsp_hall_init()` 函数 - 禁用EXTI初始化

```c
uint8_t bsp_hall_init(void)
{
    uint32_t now;
    uint8_t state;

    if (HAL_TIM_Base_Start(&htim5) != HAL_OK) {
        return BSP_HALL_ERROR_TIM5_START;
    }

    state = bsp_hall_sample_state();
    now = __HAL_TIM_GET_COUNTER(&htim5);

    s_hall_state = state;
    s_hall_last_state = state;
    s_last_commutation_time = now;
    s_last_period_ticks = 0U;
    s_edge_count = 0U;
    s_valid_transition_count = 0U;
    s_invalid_transition_count = 0U;
    s_direction = BSP_HALL_DIR_UNKNOWN;

    // ❌ 删除：bsp_hall_irq_enable();  // 不再使用EXTI中断
    return 0U;
}
```

**修改2：** 添加轮询函数

```c
/**
 * @brief 轮询霍尔传感器并在状态变化时换相
 * @note 在TIM1更新中断中调用（约18kHz频率）
 */
void bsp_hall_poll_and_commutate(void)
{
    uint8_t new_state;
    uint8_t old_state;
    uint32_t now;

    new_state = bsp_hall_sample_state();
    old_state = s_hall_state;

    if (new_state == old_state) {
        return;  // 状态未变化，直接返回
    }

    // 状态变化，执行换相逻辑
    s_edge_count++;
    s_hall_last_state = old_state;
    s_hall_state = new_state;

    if (bsp_hall_is_valid_state(old_state) && bsp_hall_is_valid_state(new_state)) {
        // 判断方向
        if (bsp_hall_get_next_state(old_state) == new_state) {
            s_direction = BSP_HALL_DIR_CCW;
        } else if (bsp_hall_get_prev_state(old_state) == new_state) {
            s_direction = BSP_HALL_DIR_CW;
        } else {
            s_direction = BSP_HALL_DIR_UNKNOWN;
            s_invalid_transition_count++;
            return;
        }

        // 更新时间和统计
        now = __HAL_TIM_GET_COUNTER(&htim5);
        s_last_period_ticks = now - s_last_commutation_time;
        s_last_commutation_time = now;
        s_valid_transition_count++;

        // 调用换相函数
        motor_sensor_mode_phase();
    } else {
        s_direction = BSP_HALL_DIR_UNKNOWN;
        s_invalid_transition_count++;
    }
}
```

**修改3：** 注释掉原来的EXTI回调

```c
/*
// 不再使用EXTI中断方式
void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin)
{
    // 整个函数注释掉
}
*/
```

#### 文件3：`Core/Src/tim.c`（CubeMX生成的文件）

**位置：** 在 `/* USER CODE BEGIN 4 */` 和 `/* USER CODE END 4 */` 之间

```c
/* USER CODE BEGIN 4 */

// 引入霍尔轮询函数
extern void bsp_hall_poll_and_commutate(void);

/**
 * @brief TIM更新中断回调
 * @note TIM1更新中断用于轮询霍尔传感器
 */
void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim)
{
    if (htim->Instance == TIM1) {
        // TIM1更新中断，轮询霍尔传感器
        bsp_hall_poll_and_commutate();
    }
}

/* USER CODE END 4 */
```

#### 文件4：`User/Motor/Src/motor_ctrl.c`

**修改：** 删除所有霍尔中断使能/禁用调用

```c
// 在 motor_ctrl_init() 中删除：
// bsp_hall_irq_enable();

// 在 motor_ctrl_prepare_start() 中删除：
// bsp_hall_irq_disable();

// 在 motor_start() 中删除：
// bsp_hall_irq_enable();

// 在 motor_start_simple() 中删除：
// bsp_hall_irq_enable();

// 在 motor_start_assisted() 中删除：
// bsp_hall_irq_enable();

// 在 motor_stop() 中删除：
// bsp_hall_irq_enable();
```

### 预期效果
- ✅ 霍尔传感器以PWM频率轮询（约18kHz）
- ✅ 对噪声/毛刺更鲁棒
- ✅ 换相时序更可预测
- ⚠️ CPU占用略微增加（但在可接受范围）

### 测试方法
1. 重新生成CubeMX代码
2. 编译烧录
3. 测试启动是否正常
4. 观察霍尔状态是否正确循环

### 回退方案
1. 在CubeMX中恢复PA0/PA1/PA2为EXTI模式
2. 恢复 `bsp_hall_init()` 中的 `bsp_hall_irq_enable()`
3. 取消注释 `HAL_GPIO_EXTI_Callback()`
4. 删除 `tim.c` 中的 `HAL_TIM_PeriodElapsedCallback()`
5. 恢复 `motor_ctrl.c` 中的中断使能/禁用调用

---

## 修改顺序建议

### 方案A：渐进式修改（推荐）

**适合：** 稳妥开发，便于定位问题

1. ✅ **先做阶段1**（简化启动）- 风险最小，改动最小
2. ✅ **测试通过后做阶段2**（PWM始终开启）- 中等改动
3. ✅ **最后做阶段3**（定时器轮询）- 改动最大，需要重新生成CubeMX代码

**优点：**
- 每个阶段独立测试
- 容易定位问题
- 可以随时回退

**缺点：**
- 需要多次编译烧录
- 总时间较长

### 方案B：一次性全部修改

**适合：** 对代码很熟悉，追求效率

1. 同时完成所有三个阶段的修改
2. 一次性编译烧录测试

**优点：**
- 只需编译烧录一次
- 节省时间

**缺点：**
- 调试困难
- 不容易定位问题
- 风险较高

---

## 关键注意事项

### 1. 备份当前代码
```bash
git add .
git commit -m "Backup before refactoring"
```

### 2. 逐步测试
每个阶段完成后都要测试，确保功能正常再进行下一阶段。

### 3. 增加占空比
简化启动后可能需要更高的初始占空比：
- 原来：15% → 18% → 20%
- 建议：25% → 30% → 35%

### 4. 提高电流限制
你的0.5A限制太低，建议提高到 **1-2A**，否则电机扭矩不足。

### 5. CubeMX代码保护
阶段3需要在 `tim.c` 中添加代码，**必须放在 USER CODE 区域**：
```c
/* USER CODE BEGIN 4 */
// 你的代码放这里
/* USER CODE END 4 */
```

### 6. 示波器验证
- 阶段2后，用示波器观察PA8/PA9/PA10应该始终有PWM波形
- 阶段3后，观察换相时序是否正常

---

## 预期改进效果

| 方面 | 改造前 | 改造后 |
|------|--------|--------|
| 启动时间 | 80ms对齐 + 6步踢启 | 立即启动 |
| 启动复杂度 | 5个阶段状态机 | 直接换相 |
| PWM控制 | 动态启停 | 始终运行 |
| 霍尔读取 | EXTI事件驱动 | 18kHz轮询 |
| 抗干扰能力 | 一般 | 更强 |
| 时序稳定性 | 一般 | 更好 |
| CPU占用 | 较低 | 略微增加 |

---

## 故障排查

### 问题1：阶段1后电机不转
**可能原因：** 初始占空比太低
**解决方案：** 增加到30-35%

### 问题2：阶段2后电机异常
**可能原因：** PWM通道未正确使能
**解决方案：** 检查 `bsp_pwm_init()` 中的启动代码

### 问题3：阶段3后编译错误
**可能原因：** CubeMX重新生成代码覆盖了USER CODE区域
**解决方案：** 确保代码在 `/* USER CODE BEGIN/END */` 之间

### 问题4：阶段3后电机不响应
**可能原因：** TIM1中断未使能或优先级太低
**解决方案：** 检查CubeMX中TIM1中断配置

---

## 总结

本方案将当前实现改造为参考实现风格，主要改进：
1. ✅ 简化启动逻辑
2. ✅ 提高时序稳定性
3. ✅ 增强抗干扰能力

**建议按顺序执行三个阶段，每个阶段测试通过后再进行下一阶段。**

---

*文档创建日期：2026-03-14*
*适用项目：My_Motor_Control_Project*
