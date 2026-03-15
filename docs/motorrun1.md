# 正点原子BLDC基础驱动实现逻辑分析

## 概述

**工作条件：** 12V 0.5A，占空比5%，电机能够正常启动并旋转

**核心特点：**
1. PWM始终运行（从初始化开始）
2. TIM1更新中断轮询霍尔传感器（约55μs周期，18kHz频率）
3. 简单直接的六步换相
4. 无复杂启动序列

---

## 1. 初始化流程

### 1.1 主函数初始化（main.c）

```c
int main(void)
{
    HAL_Init();
    sys_stm32_clock_init(85, 2, 2, 4, 8);
    delay_init(170);
    usart_init(115200);
    led_init();
    key_init();
    lcd_init();

    // BLDC初始化：配置TIM1 PWM
    bldc_init((170000 / 18) - 1, 0);  // ARR=9443, PSC=0 → 18kHz PWM

    // 设置初始状态：顺时针，占空比=0
    bldc_ctrl(MOTOR_1, CW, 0);

    // 主循环...
}
```

**关键参数：**
- `ARR = (170000 / 18) - 1 = 9443` → PWM频率 = 170MHz / 9444 ≈ **18kHz**
- `PSC = 0` → 无预分频
- 初始占空比 = 0
- 初始方向 = CW（顺时针）

### 1.2 TIM1 PWM初始化（bldc_tim.c）

```c
void atim_timx_oc_chy_init(uint16_t arr, uint16_t psc)
{
    // 1. 配置TIM1基本参数
    g_atimx_handle.Instance = TIM1;
    g_atimx_handle.Init.Prescaler = psc;              // PSC=0
    g_atimx_handle.Init.CounterMode = TIM_COUNTERMODE_UP;
    g_atimx_handle.Init.Period = arr;                 // ARR=9443
    g_atimx_handle.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
    g_atimx_handle.Init.RepetitionCounter = 0;
    HAL_TIM_PWM_Init(&g_atimx_handle);

    // 2. 配置PWM通道1/2/3
    g_atimx_oc_chy_handle.OCMode = TIM_OCMODE_PWM1;
    g_atimx_oc_chy_handle.Pulse = 0;                  // 初始CCR=0
    g_atimx_oc_chy_handle.OCPolarity = TIM_OCPOLARITY_HIGH;
    HAL_TIM_PWM_ConfigChannel(&g_atimx_handle, &g_atimx_oc_chy_handle, TIM_CHANNEL_1);
    HAL_TIM_PWM_ConfigChannel(&g_atimx_handle, &g_atimx_oc_chy_handle, TIM_CHANNEL_2);
    HAL_TIM_PWM_ConfigChannel(&g_atimx_handle, &g_atimx_oc_chy_handle, TIM_CHANNEL_3);

    // 3. ✅ 立即启动所有PWM通道
    HAL_TIM_PWM_Start(&g_atimx_handle, TIM_CHANNEL_1);
    HAL_TIM_PWM_Start(&g_atimx_handle, TIM_CHANNEL_2);
    HAL_TIM_PWM_Start(&g_atimx_handle, TIM_CHANNEL_3);

    // 4. ✅ 启动TIM1更新中断（用于霍尔轮询）
    HAL_TIM_Base_Start_IT(&g_atimx_handle);
}
```

**关键点：**
- PWM从初始化开始就**持续运行**
- TIM1更新中断**立即启动**，每55μs触发一次
- 初始CCR=0，所以PWM占空比为0%

### 1.3 BLDC软件初始化（bldc_motor.c）

```c
void bldc_init(void)
{
    bldc_hal_shutdown_disable();    // 关闭半桥芯片（SD引脚=LOW）
    bldc_hal_low_all_off();         // 下桥臂全部关断
}
```

**初始状态：**
- SD引脚 = LOW → 驱动器禁用
- 下桥臂全部关闭
- PWM运行但无输出（因为驱动器禁用）

---

## 2. 霍尔传感器读取

### 2.1 霍尔状态采样（bldc_motor.c）

```c
uint32_t hallsensor_get_state(uint8_t motor_id)
{
    uint32_t state = 0;

    if (motor_id == MOTOR_1)
    {
        if (bldc_hal_read_hall_u())  state |= 0x01U;  // bit0 = U
        if (bldc_hal_read_hall_v())  state |= 0x02U;  // bit1 = V
        if (bldc_hal_read_hall_w())  state |= 0x04U;  // bit2 = W
    }
    return state;  // 返回值：1~6（有效），0或7（无效）
}
```

**霍尔位映射：**
- bit0 = U相霍尔
- bit1 = V相霍尔
- bit2 = W相霍尔

**有效状态：** 1, 2, 3, 4, 5, 6
**无效状态：** 0 (000), 7 (111)

### 2.2 TIM1更新中断轮询（bldc_tim.c）

```c
void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim)
{
    if (htim->Instance == TIM1)  // 每55μs触发一次
    {
        if (g_bldc_motor1.run_flag == RUN)
        {
            // 读取霍尔状态
            if (g_bldc_motor1.dir == CW)  // 顺时针
            {
                g_bldc_motor1.step_sta = hallsensor_get_state(MOTOR_1);
            }
            else  // 逆时针
            {
                // 使用7-霍尔值来反转序列
                g_bldc_motor1.step_sta = 7 - hallsensor_get_state(MOTOR_1);
            }

            // 检查霍尔状态有效性
            if ((g_bldc_motor1.step_sta <= 6) && (g_bldc_motor1.step_sta >= 1))
            {
                // 调用对应的换相函数
                pfunclist_m1[g_bldc_motor1.step_sta - 1]();
            }
            else  // 无效霍尔状态 → 立即停止
            {
                stop_motor1();
                g_bldc_motor1.run_flag = STOP;
            }
        }
    }
}
```

**轮询频率：** 18kHz（每55μs）
**触发条件：** `run_flag == RUN`
**方向处理：** CCW使用 `7 - hall_state` 反转序列

---

## 3. 六步换相逻辑

### 3.1 换相表（H_PWM_L_ON模式）

```c
// 函数指针数组
pctr pfunclist_m1[6] = {
    &m1_uhwl,  // [0] 霍尔=1 → U+W-
    &m1_vhul,  // [1] 霍尔=2 → V+U-
    &m1_vhwl,  // [2] 霍尔=3 → V+W-
    &m1_whvl,  // [3] 霍尔=4 → W+V-
    &m1_uhvl,  // [4] 霍尔=5 → U+V-
    &m1_whul   // [5] 霍尔=6 → W+U-
};
```

**CW顺时针序列：** 1 → 2 → 3 → 4 → 5 → 6 → 1...
**CCW逆时针序列：** 使用 `7-hall` 反转

### 3.2 换相函数实现（以U+W-为例）

```c
void m1_uhwl(void)  // 霍尔=1 → U相上桥臂PWM，W相下桥臂导通
{
    // 1. 设置上桥臂PWM占空比
    bldc_hal_set_duty_u(g_bldc_motor1.pwm_duty);  // U相PWM
    bldc_hal_set_duty_v(0);                       // V相关闭
    bldc_hal_set_duty_w(0);                       // W相关闭

    // 2. 设置下桥臂GPIO
    bldc_hal_low_w_on();   // W相下桥臂导通
    bldc_hal_low_u_off();  // U相下桥臂关闭
    bldc_hal_low_v_off();  // V相下桥臂关闭
}
```

**换相动作：**
1. 只有一个上桥臂通道输出PWM
2. 只有一个下桥臂GPIO置高
3. 其他通道全部关闭

**完整换相表：**

| 霍尔值 | 上桥臂PWM | 下桥臂GPIO | 电流路径 |
|--------|-----------|------------|----------|
| 1      | U         | W          | U+ → W-  |
| 2      | V         | U          | V+ → U-  |
| 3      | V         | W          | V+ → W-  |
| 4      | W         | V          | W+ → V-  |
| 5      | U         | V          | U+ → V-  |
| 6      | W         | U          | W+ → U-  |

---

## 4. 启动流程

### 4.1 按键启动（main.c）

```c
if (key == KEY0_PRES)  // KEY0按下
{
    stop_motor1();  // 先停止（确保安全状态）

    pwm_duty_temp += 500;  // 增加占空比

    if (pwm_duty_temp >= MAX_PWM_DUTY / 2)
    {
        pwm_duty_temp = MAX_PWM_DUTY / 2;  // 限制最大50%
    }

    // 设置电机参数
    g_bldc_motor1.pwm_duty = pwm_duty_temp;
    g_bldc_motor1.dir = CW;
    g_bldc_motor1.run_flag = RUN;  // ✅ 设置运行标志

    start_motor1();  // 启动电机
}
```

**启动步骤：**
1. 停止电机（安全）
2. 设置占空比（每次按键+500，约5%）
3. 设置方向（CW）
4. **设置运行标志** `run_flag = RUN`
5. 启动电机

### 4.2 启动函数（bldc_motor.c）

```c
void start_motor1(void)
{
    bldc_hal_shutdown_enable();   // 使能半桥芯片（SD=HIGH）
    bldc_hal_pwm_start_all();     // 启动PWM输出（实际上已经在运行）
}
```

**关键点：**
- PWM已经在运行，这里只是确保使能
- **SD引脚置高** → 驱动器输出使能
- TIM1中断会立即开始换相（因为`run_flag=RUN`）

### 4.3 首次换相

```
TIM1中断（55μs后）
  ↓
读取当前霍尔状态（例如：霍尔=1）
  ↓
调用 pfunclist_m1[0]() → m1_uhwl()
  ↓
U相上桥臂PWM，W相下桥臂导通
  ↓
电机开始转动
  ↓
霍尔状态变化（1→2）
  ↓
下次中断换相...
```

**无需对齐或踢启：** 直接根据当前霍尔状态换相

---

## 5. 停止流程

```c
void stop_motor1(void)
{
    bldc_hal_shutdown_disable();  // 关闭半桥芯片（SD=LOW）
    bldc_hal_pwm_stop_all();      // 停止PWM（实际上不停止，只是禁用输出）

    // 上桥臂CCR全部清零
    bldc_hal_set_duty_u(0);
    bldc_hal_set_duty_v(0);
    bldc_hal_set_duty_w(0);

    // 下桥臂全部关断
    bldc_hal_low_all_off();
}
```

**停止步骤：**
1. SD引脚置低 → 驱动器禁用
2. 清零所有CCR
3. 关闭所有下桥臂
4. PWM定时器继续运行（但无输出）

---

## 6. 关键差异对比

### 6.1 与你当前实现的差异

| 方面 | 正点原子实现 | 你的实现 | 影响 |
|------|-------------|----------|------|
| **PWM启动** | 初始化时立即启动 | ✅ 已修改为立即启动 | 无差异 |
| **TIM1中断** | 初始化时启动 | ✅ 已添加 | 无差异 |
| **霍尔轮询** | TIM1中断中轮询 | ✅ 已实现 | 无差异 |
| **换相触发** | 每次中断都检查 | ✅ 已实现 | 无差异 |
| **启动序列** | 无，直接换相 | ✅ 已简化 | 无差异 |
| **方向反转** | 使用 `7-hall` | 使用独立表 | ⚠️ 可能有问题 |
| **换相表** | 函数指针数组 | 查找表+switch | ⚠️ 可能有问题 |
| **霍尔序列** | 1→2→3→4→5→6 | 6→2→3→1→5→4 | ❌ **关键差异** |

### 6.2 发现的问题

#### **问题1：霍尔序列不匹配**

**正点原子：**
```c
// CW顺时针序列
霍尔值：1 → 2 → 3 → 4 → 5 → 6 → 1...
换相：  U+W- → V+U- → V+W- → W+V- → U+V- → W+U-
```

**你的实现：**
```c
// CW顺时针序列（motor_phase_tab.c）
const uint8_t s_hall_sequence_cw[6] = {
    0x06U, 0x02U, 0x03U, 0x01U, 0x05U, 0x04U  // 6→2→3→1→5→4
};
```

**这是不同的序列！** 你的序列是从6开始，而参考实现是从1开始。

#### **问题2：换相表映射可能不正确**

你的换相表（motor_phase_tab.c）：
```c
s_phase_table[0][0x01U] = MOTOR_PHASE_ACTION_WP_VN;  // Hall 1 → W+V-
s_phase_table[0][0x02U] = MOTOR_PHASE_ACTION_VP_UN;  // Hall 2 → V+U-
s_phase_table[0][0x03U] = MOTOR_PHASE_ACTION_VP_WN;  // Hall 3 → V+W-
s_phase_table[0][0x04U] = MOTOR_PHASE_ACTION_WP_UN;  // Hall 4 → W+U-
s_phase_table[0][0x05U] = MOTOR_PHASE_ACTION_UP_VN;  // Hall 5 → U+V-
s_phase_table[0][0x06U] = MOTOR_PHASE_ACTION_UP_WN;  // Hall 6 → U+W-
```

参考实现的换相表：
```c
pfunclist_m1[0] = m1_uhwl;  // 霍尔=1 → U+W-
pfunclist_m1[1] = m1_vhul;  // 霍尔=2 → V+U-
pfunclist_m1[2] = m1_vhwl;  // 霍尔=3 → V+W-
pfunclist_m1[3] = m1_whvl;  // 霍尔=4 → W+V-
pfunclist_m1[4] = m1_uhvl;  // 霍尔=5 → U+V-
pfunclist_m1[5] = m1_whul;  // 霍尔=6 → W+U-
```

**对比：**
- 霍尔1：你的=W+V-，参考=U+W- ❌ **不匹配**
- 霍尔2：你的=V+U-，参考=V+U- ✅ 匹配
- 霍尔3：你的=V+W-，参考=V+W- ✅ 匹配
- 霍尔4：你的=W+U-，参考=W+V- ❌ **不匹配**
- 霍尔5：你的=U+V-，参考=U+V- ✅ 匹配
- 霍尔6：你的=U+W-，参考=W+U- ❌ **不匹配**

---

## 7. 需要修改的配置

### 7.1 修改换相表（motor_phase_tab.c）

**当前（错误）：**
```c
s_phase_table[0][0x01U] = MOTOR_PHASE_ACTION_WP_VN;  // Hall 1 → W+V-
s_phase_table[0][0x04U] = MOTOR_PHASE_ACTION_WP_UN;  // Hall 4 → W+U-
s_phase_table[0][0x06U] = MOTOR_PHASE_ACTION_UP_WN;  // Hall 6 → U+W-
```

**应改为（参考实现）：**
```c
s_phase_table[0][0x01U] = MOTOR_PHASE_ACTION_UP_WN;  // Hall 1 → U+W-
s_phase_table[0][0x04U] = MOTOR_PHASE_ACTION_WP_VN;  // Hall 4 → W+V-
s_phase_table[0][0x06U] = MOTOR_PHASE_ACTION_WP_UN;  // Hall 6 → W+U-
```

### 7.2 修改霍尔序列（motor_phase_tab.c）

**当前（可能错误）：**
```c
static const uint8_t s_hall_sequence_cw[6] = {
    0x06U, 0x02U, 0x03U, 0x01U, 0x05U, 0x04U  // 6→2→3→1→5→4
};
```

**应改为（参考实现）：**
```c
static const uint8_t s_hall_sequence_cw[6] = {
    0x01U, 0x02U, 0x03U, 0x04U, 0x05U, 0x06U  // 1→2→3→4→5→6
};
```

### 7.3 修改CCW序列（motor_phase_tab.c）

**当前：**
```c
static const uint8_t s_hall_sequence_ccw[6] = {
    0x06U, 0x04U, 0x05U, 0x01U, 0x03U, 0x02U  // 6→4→5→1→3→2
};
```

**应改为（参考实现使用7-hall反转）：**
```c
// CCW = 7 - CW
// CW: 1→2→3→4→5→6
// 7-CW: 6→5→4→3→2→1
static const uint8_t s_hall_sequence_ccw[6] = {
    0x06U, 0x05U, 0x04U, 0x03U, 0x02U, 0x01U  // 6→5→4→3→2→1
};
```

---

## 8. 总结

### 8.1 参考实现的核心优势

1. **简单直接** - 无复杂启动序列
2. **高频轮询** - 18kHz轮询，对噪声鲁棒
3. **PWM持续运行** - 时序稳定
4. **立即响应** - 根据当前霍尔状态直接换相

### 8.2 你的实现需要修改的地方

1. ❌ **换相表映射错误** - 霍尔1/4/6的映射不正确
2. ❌ **霍尔序列错误** - 应该是1→2→3→4→5→6，不是6→2→3→1→5→4
3. ⚠️ **CCW序列可能错误** - 应该是6→5→4→3→2→1

### 8.3 修改优先级

**立即修改（关键）：**
1. 换相表中霍尔1/4/6的映射
2. CW霍尔序列改为1→2→3→4→5→6
3. CCW霍尔序列改为6→5→4→3→2→1

**已完成（无需修改）：**
1. ✅ PWM始终开启
2. ✅ TIM1更新中断轮询
3. ✅ 简化启动序列

---

*文档创建日期：2026-03-15*
*参考工程：正点原子 BLDC基础驱动*
