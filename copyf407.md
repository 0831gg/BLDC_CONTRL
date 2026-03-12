# BLDC Hall 复刻检查与实施方案

> 目标：将 `D:\Github DownLoad\motorcontrol\bldc_hall_f407` 的裸机/标准库 BLDC Hall 控制代码，迁移到 `D:\Electrical Design\MOTOR\My_Motor_Control_Project\my_control` 这个基于 CubeMX + HAL 的 STM32G474 工程。

---

## 1. 现有 CubeMX 配置是否满足复刻需求

先说结论：

- 你现在的 `BLDC_HALL.ioc` 已经把大部分“硬件基础资源”搭起来了。
- 作为 **BLDC 六步有感换相** 的移植底座，当前工程已经具备可继续推进的条件。
- 但还 **没有完全达到“可直接复刻原工程控制逻辑”的状态**，仍有几项关键配置需要补齐或修正。

### 1.1 已具备的配置

当前工程中已经配置好的资源如下：

| 功能 | 当前状态 | 说明 |
|------|----------|------|
| TIM1 三路 PWM | 已有 | `PA8/PA9/PA10 -> TIM1_CH1/2/3`，适合三相上桥臂 PWM 输出 |
| TIM1 Break 输入 | 已有 | `PB12 -> TIM1_BKIN`，可做过流/急停保护输入 |
| Hall 三路输入 | 已有 | `PA0/PA1/PA2` 已配置为双边沿 EXTI |
| 母线电压采样 | 已有 | `ADC1 CH4 -> PA3(VBUS)` |
| 温度采样 | 已有 | `ADC1 CH6 -> PC0(TEMP)` |
| 三相电流采样 | 已有 | `ADC2 Injected CH7/8/9 -> PC1/PC2/PC3` |
| USART 调试口 | 已有 | `USART1 TX/RX + TX DMA`，适合日志和 Vofa 输出 |
| 按键输入 | 已有 | `PE12/13/14 -> KEY0/KEY1/KEY2` |
| LED 指示 | 已有 | `PE0/PE1` |
| PWM 使能脚 | 已有 | `PC13 -> PWM_Enable` |
| 下桥臂控制 GPIO | 已有 | `PB13/PB14/PB15` 可用于六步换相低桥导通 |
| 速度测量定时器 | 已有 | `TIM5` 已配置为自由运行基准计时器 |

这些资源与原工程核心需求基本一致，所以从“引脚和外设种类”角度看，方向是对的。

### 1.2 缺少或需要修正的配置

以下几项是我认为在正式加驱动文件前必须重点处理的：

| 项目 | 当前情况 | 建议处理 | 原因 |
|------|----------|----------|------|
| ADC2 注入触发源 | 目前是软件触发 | 改成 `TIM1` 触发 | 电流采样要与 PWM 周期同步，否则后面闭环/保护数据时序不稳定 |
| TIM1 死区时间 | `DeadTime = 0` | 依据驱动器需求设置 | 若后续切换到互补输出或存在上下桥切换风险，建议提前保留余量 |
| Break 极性/输出恢复策略 | 配置为 **HIGH**（高电平触发） | 硬件实际为 **低电平有效**，但因过流检测过于灵敏，故意反设为高电平以避免误触发 | 后续调试稳定后可改回 LOW，或在软件中做消抖/阈值调整 |
| TIM1 CH4 触发策略 | 现在为 `OC Timing` | 明确是否保留为 ADC 采样触发参考点 | 原工程用 CH4/触发输出参与 ADC 节拍控制，HAL 工程要明确采用 `TRGO_UPDATE` 还是 `OC4REF` |
| TIM5 中断 | 目前只初始化了 Base，无中断入口 | 若沿用原方案，需要开更新中断或改为直接读 CNT | 原工程靠 10us 节拍累加计时做转速计算 |
| Hall 输入上下拉 | 目前 `GPIO_NOPULL` | 依据霍尔输出方式确认是否改为上拉 | 原工程 Hall 输入为上拉输入，若传感器为开漏/集电极开路，当前配置会有风险 |
| 按键给定方式定义 | 已有 3 键，但原工程还有速度旋钮 | 明确“按键调速替代旋钮” | 后续 `analog_calculate` 和 `motor_execute` 的输入来源要统一 |
| ADC1 通道数量 | 仅 2 路 | 当前可接受 | 原工程规则通道含速度旋钮；你这里如果改按键调速，则不必补第三路 ADC |
| UART RX 回调链路 | USART 已开，但用户层还没接好 | 后续按需补 `HAL_UART_RxCpltCallback` | 若只做打印/Vofa，可先不做 RX |

### 1.3 与原工程相比，不必强行照搬的部分

原工程有几项内容不必一比一复刻：

- `bsp_systick.c`：HAL 已有 `HAL_GetTick()`，微秒延时可继续保留你现有 `bsp_delay.c`
- `bsp_io.c`：CubeMX 已生成 `MX_GPIO_Init()`，不需要再保留一套独立 IO 初始化
- `bsp_dac.c`：你已有 Vofa 串口链路，调试输出不一定要 DAC
- 速度电位器 ADC 通道：你当前更适合改成按键给定速度

### 1.4 结论

当前 CubeMX 配置：

- **能作为复刻起点**
- **不足以直接上电跑完整控制逻辑**
- 在继续移植前，建议先补齐以下最小修改项：

1. ADC2 注入转换改为 `TIM1` 触发
2. 明确 TIM1 触发输出方案（`TRGO_UPDATE` 或 `OC4REF`）
3. 确认 Hall 输入上下拉方式
4. 明确 TIM5 作为自由运行计时器的使用方式
5. 确认 Break 极性、自动恢复、`PWM_Enable` 默认电平

### 1.5 CubeMX 配置重新验证结果 (2026-03-12)

在用户修改 CubeMX 后（下桥臂改为 GPIO 翻转，TIM1 参数调整），重新读取生成代码验证如下：

#### ✅ 已确认正确

| 项目 | 验证值 | 说明 |
|------|--------|------|
| TIM1 PSC=0, ARR=4199 | ✓ | 168MHz/1/2/4200 = **20kHz** 中心对齐 |
| TIM1 CH1/2/3 PWM1 + CH4 OC Timing | ✓ | 上桥PWM + ADC触发时基保留 |
| TIM1 BKIN (PB12, HIGH极性) | ✓ | Break中断已使能，优先级0。硬件实际为低电平有效，故意配置为高电平防止过流误触发 |
| TIM1 DeadTime = 0 | ✓ | GPIO 翻转下桥不需要死区 |
| TIM1 TRGO = Update | ✓ | 替代源项目的 OC4Ref 方案，效果等价 |
| TIM1 AutomaticOutput = DISABLE | ✓ | Break后需软件恢复，比源项目(AUTO_ENABLE)更安全 |
| TIM5 PSC=1679, Period=0xFFFFFFFF | ✓ | 32位自由运行 100kHz(10µs分辨率)，43000秒才溢出，**无需溢出中断** |
| ADC1 CH4(VBUS)+CH6(TEMP) DMA循环 | ✓ | DMA1_CH2 自动刷新 |
| ADC2 注入 CH7/8/9 (三相电流) | ✓ 通道正确 | 但触发源仍需修改 ⬇ |
| EXTI0/1/2 Hall双边沿 | ✓ | PA0/1/2，优先级1，各自独立中断线 |
| PB13/14/15 下桥GPIO | ✓ | 输出PP, HIGH速, 初始RESET |
| PC13 PWM_Enable | ✓ | 初始SET (高=禁止驱动器输出) |
| 系统时钟 168MHz | ✓ | HSI+PLL: M=1, N=21, R=2 |

#### ⚠️ 仍需修改（Phase 2 ADC采集测试前必须完成）

| 项目 | 当前值 | 应改为 |
|------|--------|--------|
| **ADC2 注入触发源** | `ADC_INJECTED_SOFTWARE_START` | `Timer 1 Trigger Out event` + Rising Edge |

CubeMX 操作路径：ADC2 → Injected Conversion → External Trigger Source → **Timer 1 Trigger Out event** → Edge = Rising

#### 📋 与源项目的有意差异

| 差异项 | 源(F407) | 目标(G474) | 影响 |
|--------|----------|-----------|------|
| Break极性 | LOW(低电平触发) | **HIGH**(高电平触发) | 因过流检测灵敏度过高故意反设，后续可调回 |
| OSSI/OSSR | Enable | Disable | 目标更安全 |
| AutomaticOutput | Enable(自动恢复) | Disable(需软件恢复) | 目标更安全 |
| TRGO来源 | OC4Ref | Update | 效果等价，都同步 PWM 周期 |
| 速度定时器 | TIM4(16位+溢出中断累加) | **TIM5(32位自由运行)** | 大幅简化，无需10µs中断 |

---

## 2. 原工程回调函数在 HAL 工程中怎么处理

原工程把“外设主体逻辑”和“中断回调逻辑”拆成了两类文件：

- `bsp_xxx.c`
- `bsp_xxx_cb.c`

这种写法在标准库里常见，但在 HAL + CubeMX 工程里不建议照搬。

HAL 的更合适做法是：

- 中断入口仍然放在 CubeMX 生成的 `Core/Src/stm32g4xx_it.c`
- 在入口中调用 `HAL_xxx_IRQHandler(...)`
- 业务处理统一写进 HAL 的弱回调函数里
- 回调代码直接合并进对应 BSP 源文件，不单独保留 `_cb.c`

### 2.1 推荐的合并关系

| 原文件 | 建议在新工程中的归属 | 推荐 HAL 回调 |
|--------|----------------------|---------------|
| `bsp_hall_exti_cb.c` | 合并进 `bsp_hall.c` | `HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin)` |
| `bsp_hall_speed_cb.c` | 合并进 `bsp_hall.c` | `HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim)` |
| `bsp_pwm_cb.c` | 合并进 `bsp_pwm.c` | `HAL_TIMEx_BreakCallback(TIM_HandleTypeDef *htim)` |
| `bsp_adc_cb.c` | 合并进 `bsp_adc_motor.c` | `HAL_ADCEx_InjectedConvCpltCallback(ADC_HandleTypeDef *hadc)` |
| `bsp_uart_cb.c` | 合并进 `bsp_uart.c` | `HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart)` |

### 2.2 各回调如何对应改写

#### Hall 外部中断

原工程本质是：

- U/V/W 任意一个 Hall 变化
- 调一次 `motro_sensor_mode_phase()` 或同类换相算法

HAL 下建议写成：

```c
void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin)
{
    if ((GPIO_Pin == HALL_U_Pin) ||
        (GPIO_Pin == HALL_V_Pin) ||
        (GPIO_Pin == HALL_W_Pin))
    {
        motor_sensor_mode_phase();
    }
}
```

这样就不需要保留 `hall_uvw_irq_cb[]` 这一层转发数组，逻辑更直接。

#### Hall 速度定时

原工程 `bsp_hall_speed_cb.c` 的实质是：

- 定时器每 10us 进一次更新中断
- 软件计数器 `timer_us_counter++`

HAL 下有两种实现方式：

1. 保留原思路：开启 TIM5 更新中断，在 `HAL_TIM_PeriodElapsedCallback()` 里累加软计数器
2. 更推荐：TIM5 自由运行，不开高频中断，直接在 Hall 变化时读取 `__HAL_TIM_GET_COUNTER(&htim5)` 计算间隔

对于 G474 项目，我更推荐第 2 种，因为：

- CPU 开销更小
- 不需要 10us 高频中断
- 更适合后续闭环控制扩展

也就是说，`bsp_hall_speed_cb.c` 不一定要“机械复刻”，可以直接被新的时间戳测量方案替代。

#### PWM Break 回调

原工程 `bsp_pwm_cb.c` 只是做了一个空的 Break 中断处理框架。HAL 下对应：

```c
void HAL_TIMEx_BreakCallback(TIM_HandleTypeDef *htim)
{
    if (htim->Instance == TIM1)
    {
        motor_ctrl_set_fault(MOTOR_FAULT_BREAK);
        bsp_pwm_all_close();
        bsp_ctrl_sd_disable();
    }
}
```

这个回调应放在 `bsp_pwm.c` 或 `motor_ctrl.c` 相关模块内，不需要单独 `_cb.c`。

#### ADC 注入转换完成回调

原工程 `bsp_adc_cb.c` 做了三件事：

1. 读取注入通道结果
2. 读取规则通道缓冲值
3. 基于注入中断触发后续规则转换

你当前工程中：

- `ADC1` 已经是 DMA + 连续规则转换
- `ADC2` 是注入组

所以在 HAL 中要调整思路：

- 规则组数据直接从 DMA 缓冲区读
- 注入组数据在 `HAL_ADCEx_InjectedConvCpltCallback()` 中更新
- 不需要再像原工程那样“在注入回调里手工触发 ADC1 规则转换”

也就是说，这个回调应当“保留数据采集动作”，但“删除原先的软件触发耦合”。

#### UART 回调

原工程 `bsp_uart_cb.c` 基本是空实现。

你当前 `User/Bsp/Src/bsp_uart.c` 几乎还是空文件，所以建议：

- 如果前期只需要 `printf` / Vofa 发送，可以先不做 UART RX 回调
- 若后续要串口命令调速，再在 `bsp_uart.c` 里补 `HAL_UART_RxCpltCallback()`

### 2.3 标准库 → HAL 关键 API 对照表

移植过程中最常用的函数对照：

| 功能 | 源 (StdPeriph F407) | 目标 (HAL G474) |
|------|--------------------|-----------------|
| 下桥控制 | `GPIO_WriteBit(GPIOB, GPIO_Pin_13, level)` | `HAL_GPIO_WritePin(GPIOB, GPIO_PIN_13, level)` |
| 读Hall值 | `GPIO_ReadInputDataBit(GPIOH, GPIO_Pin_10)` | `HAL_GPIO_ReadPin(GPIOA, HALL_U_Pin)` |
| 设置占空比 | `TIM1->CCR1 = duty` | `__HAL_TIM_SET_COMPARE(&htim1, TIM_CHANNEL_1, duty)` 或直接写寄存器 |
| PWM单相使能 | `TIM_CCxCmd(TIM1, TIM_Channel_1, Enable)` | `TIM1->CCER \|= TIM_CCER_CC1E` |
| PWM单相禁止 | `TIM_CCxCmd(TIM1, TIM_Channel_1, Disable)` | `TIM1->CCER &= ~TIM_CCER_CC1E` |
| PWM全开 | `TIM_CCxCmd() ×3 Enable` | `HAL_TIM_PWM_Start(&htim1, TIM_CHANNEL_x)` ×3 |
| PWM全关 | `TIM_CCxCmd() ×3 Disable` | `HAL_TIM_PWM_Stop(&htim1, TIM_CHANNEL_x)` ×3 |
| 主输出使能 | `TIM_CtrlPWMOutputs(TIM1, ENABLE)` | `__HAL_TIM_MOE_ENABLE(&htim1)` |
| 读注入ADC | `ADC_GetInjectedConversionValue(ADC3, ch)` | `HAL_ADCEx_InjectedGetValue(&hadc2, rank)` |
| 读速度计数 | `timer_us_counter` (软件溢出累加) | `__HAL_TIM_GET_COUNTER(&htim5)` (32位直读) |
| SysTick时间 | `bsp_systick_time_get()` | `HAL_GetTick()` (返回ms) |
| Hall引脚 | `PH10(U)/PH11(V)/PH12(W)` → 共享 `EXTI15_10` | `PA0(U)/PA1(V)/PA2(W)` → 独立 `EXTI0/1/2` |

### 2.4 HAL 弱函数跨文件冲突说明

HAL 弱函数（如 `HAL_GPIO_EXTI_Callback`）在整个工程中**只能定义一次**。当前计划中各回调分布如下，不会冲突：

| HAL 弱函数 | 定义位置 | 职责 |
|-----------|---------|------|
| `HAL_GPIO_EXTI_Callback()` | `bsp_hall.c` | Hall 换相触发 |
| `HAL_TIMEx_BreakCallback()` | `bsp_pwm.c` | Break 紧急停机 |
| `HAL_ADCEx_InjectedConvCpltCallback()` | `bsp_adc_motor.c` | 三相电流读取 |

如果后续有多个模块需要同一个回调（例如 `HAL_TIM_PeriodElapsedCallback` 被多个定时器使用），则需要在一个统一入口中按 `htim->Instance` 分发。

### 2.5 哪些文件应该删除，哪些不要再建

如果按 HAL 风格整理，建议不要再单独创建这些文件：

- `bsp_hall_exti_cb.c`
- `bsp_hall_speed_cb.c`
- `bsp_pwm_cb.c`
- `bsp_adc_cb.c`
- `bsp_uart_cb.c`

另外，这几个原工程文件也不建议照搬：

- `bsp_systick.c`
- `bsp_io.c`
- `bsp_dac.c`

### 2.6 推荐保留/新增的文件组织方式

建议把用户层重新整理成下面这种结构：

```text
User/
  Bsp/
    Inc/
      bsp_pwm.h
      bsp_hall.h
      bsp_adc_motor.h
      bsp_ctrl_sd.h
      bsp_uart.h
      bsp_led.h
      bsp_key.h
      bsp_delay.h
    Src/
      bsp_pwm.c
      bsp_hall.c
      bsp_adc_motor.c
      bsp_ctrl_sd.c
      bsp_uart.c
      bsp_led.c
      bsp_key.c
      bsp_delay.c

  Motor/
    Inc/
      motor_ctrl.h
      motor_execute.h
      motor_sensor.h
      motor_phase_tab.h
    Src/
      motor_ctrl.c
      motor_execute.c
      motor_sensor.c
      motor_phase_tab.c

  PID/
    Inc/
      pid.h
    Src/
      pid.c

  App/
    Inc/
      analog_calculate.h
    Src/
      analog_calculate.c
```

---

## 3. 如果要在“逐步测试”的条件下添加驱动文件，应该怎么加

这一部分最关键。不要一次性把全部驱动搬进去，然后一起调。正确方式是：

- 每加一类驱动，就建立对应测试项
- 每通过一层，再加下一层
- 始终保持“当前工程可编译、可上电、可验证”

下面给出推荐的分阶段流程。

### Phase 0：在继续移植前，先做现有工程基础验收

这一步你现在就应该先做，还不需要新增电机驱动文件。

#### 目标

确认现有 CubeMX 工程不是“只有配置、没有实测”的状态。

#### 建议测试项

1. LED 翻转测试
2. 按键扫描测试
3. USART1 打印测试
4. TIM1 基本 PWM 输出测试
5. ADC1 DMA 原始值采样测试
6. Hall 三路 EXTI 中断触发测试

#### 通过标准

- LED 正常闪烁
- 按键按下有串口事件打印
- PWM 有稳定输出
- ADC1 缓冲区值变化合理
- 手动拨动 Hall 线时可触发 EXTI

如果这一步都没过，不建议继续加电机算法文件。

### Phase 1：先加最底层执行相关 BSP

优先新增：

- `bsp_ctrl_sd.h/.c`
- `bsp_pwm.h/.c`

#### 只做什么

- 驱动器使能/失能
- 三路 PWM 占空比设置
- 三个低桥 GPIO 开关控制
- 全关断接口
- Break 保护接口

#### 本阶段测试

1. `PC13` 能正确使能/关闭驱动
2. `PA8/PA9/PA10` 输出 20kHz PWM
3. `PB13/PB14/PB15` 可单独高低电平控制
4. Break 拉到触发态后，PWM 立即关闭

#### 通过标准

- 示波器看到频率、占空比、关断行为都正确

### Phase 2：再加采样驱动

新增：

- `bsp_adc_motor.h/.c`
- `analog_calculate.h/.c` 先只保留转换，不上保护策略

#### 只做什么

- 建立 ADC1 DMA 缓冲区
- 建立 ADC2 注入采样值缓存
- 提供 `GetVBUS()`、`GetTemp()`、`GetPhaseCurrent()` 之类接口

#### 本阶段测试

1. 串口打印 VBUS/TEMP
2. Vofa 实时看 VBUS/TEMP 曲线
3. ADC2 注入通道值在静态条件下稳定
4. 确认 TIM1 触发后，ADC2 注入采样频率符合预期

#### 通过标准

- 电压温度数据稳定
- 三相采样无明显乱跳

### Phase 3：加 Hall 位置和速度模块

新增：

- `bsp_hall.h/.c`
- `motor_sensor.h/.c` 的“传感器部分”

#### 只做什么

- 读取 UVW 状态
- 转换为 1~6 的 Hall 扇区码
- 计算相邻跳变间隔
- 估算转速 RPM

#### 本阶段测试

1. 手转电机，打印 Hall 原始三位值
2. 打印换算后的扇区号
3. 打印 RPM
4. 正反方向都验证顺序

#### 通过标准

- Hall 顺序稳定，无非法码频繁出现
- RPM 随手转快慢变化明显

### Phase 4：加六步换相表，但先不开闭环

新增：

- `motor_phase_tab.h/.c`
- `motor_ctrl.h/.c` 最小化版本

#### 只做什么

- 根据 Hall 状态输出六步换相表
- 设固定占空比
- 支持启停和方向切换

#### 本阶段测试

1. 固定 5%~10% 占空比启动
2. 电机能否连续转动
3. 换相是否平顺
4. 正反转是否可切换

#### 通过标准

- 电机可稳定开环运行
- 无明显发抖、乱相、过流

### Phase 5：再加执行层逻辑

新增：

- `motor_execute.h/.c`

#### 只做什么

- 按键控制启停
- 方向切换
- 速度给定斜坡
- 简单堵转/超时处理

#### 本阶段测试

1. `KEY0` 启停
2. `KEY1` 换向
3. `KEY2` 负责加减速或模式切换
4. 启停过程不过流、不失步

#### 通过标准

- 按键操作与电机行为一致
- 没有明显的危险状态切换

### Phase 6：最后再加闭环和保护

新增：

- `pid.h/.c`
- `analog_calculate.c` 的保护判定完整逻辑

#### 只做什么

- 目标转速闭环
- 欠压/过压/过温保护
- Break/堵转统一故障管理

#### 本阶段测试

1. 阶跃调速
2. 轻载/重载恢复
3. 人为提高供电或温度阈值触发保护逻辑
4. 故障后停机和恢复流程测试

#### 通过标准

- 电机能稳定跟踪目标速度
- 保护动作明确且可复位

---

## 4. 根据你现在已添加的文件，建议的实施方案

你现在用户层已经存在这些文件：

- `User/Bsp/Inc/bsp_delay.h`
- `User/Bsp/Inc/bsp_key.h`
- `User/Bsp/Inc/bsp_led.h`
- `User/Bsp/Inc/bsp_uart.h`
- `User/Bsp/Src/bsp_delay.c`
- `User/Bsp/Src/bsp_key.c`
- `User/Bsp/Src/bsp_led.c`
- `User/Bsp/Src/bsp_uart.c`
- `User/Vofa/...`

其中比较明确的状态是：

- `bsp_key.c` 已经有一套较完整的按键事件机，可继续沿用
- `bsp_uart.c` 目前几乎为空，需要补初始化/发送封装
- `Vofa` 相关文件已存在，适合做阶段性数据观测

所以我建议你不要完全照抄原工程的 BSP，而是采用“保留已有、只补缺口”的策略。

### 4.1 建议保留的已有模块

- `bsp_key.*`：继续保留，用它替代原工程简单按键扫描
- `bsp_led.*`：继续保留
- `bsp_delay.*`：继续保留，用于短延时和测试脉冲
- `Vofa` 模块：继续保留，作为观察工具

### 4.2 建议新增的最小文件集合

在正式开始移植时，第一批只新增以下文件：

```text
User/Bsp/Inc/bsp_ctrl_sd.h
User/Bsp/Src/bsp_ctrl_sd.c
User/Bsp/Inc/bsp_pwm.h
User/Bsp/Src/bsp_pwm.c
User/Bsp/Inc/bsp_hall.h
User/Bsp/Src/bsp_hall.c
User/Bsp/Inc/bsp_adc_motor.h
User/Bsp/Src/bsp_adc_motor.c
```

先不要急着加：

- `motor_ctrl.*`
- `motor_execute.*`
- `motor_sensor.*`
- `analog_calculate.*`
- `pid.*`

原因很简单：

- 你现在还没完成底层硬件链路自测
- 先加太多文件，只会把问题叠在一起

### 4.3 在新增这些文件前，还应做哪些测试

这个问题你问得非常对。我的判断是：

- **在继续添加电机控制文件前，必须先做一轮“现有工程功能体检”**

建议至少完成下面 5 项再继续：

#### 必做测试 1：GPIO 与按键验证

- 验证 `LED0/LED1`
- 验证 `KEY0/KEY1/KEY2`
- 确认 `PC13(PWM_Enable)` 默认状态和实际板级逻辑一致
- 确认 `PB13/PB14/PB15` 输出不会误触发驱动器

#### 必做测试 2：TIM1 PWM 空载输出验证

- 不接电机或先断开功率级
- 启动 TIM1 PWM
- 用示波器看 `PA8/PA9/PA10`
- 确认频率、占空比和启动/停止动作

#### 必做测试 3：ADC1 DMA 验证

- 打印 `VBUS/TEMP`
- 上电后确认值是否合理
- 用手触摸温度探头附近或改变母线电压，确认值能变化

#### 必做测试 4：Hall 输入验证

- 不上驱动，只采 Hall
- 手动转动电机或模拟输入
- 确认 EXTI 回调稳定触发
- 确认读取到的 UVW 状态变化正确

#### 必做测试 5：USART/Vofa 链路验证

- 串口打印系统状态
- Vofa 至少先发 2~4 路测试数据
- 确认波形接收稳定

如果这五项里有任意一项没通过，不建议继续加换相驱动。

---

## 5. 建议的具体测试顺序

为了让你后面执行更顺，给你一个可直接照着走的顺序：

1. 先只改 `main.c`，做 LED、按键、串口、ADC、Hall、PWM 的基础测试
2. 修正 CubeMX 配置：重点是 ADC2 触发、TIM1 触发策略、Hall 上下拉确认
3. 新增 `bsp_ctrl_sd` + `bsp_pwm`，验证功率级控制接口
4. 新增 `bsp_adc_motor`，验证采样链路
5. 新增 `bsp_hall`，验证 Hall 位置和速度
6. 新增 `motor_phase_tab` + `motor_ctrl`，先跑固定占空比开环六步
7. 再加 `motor_execute`
8. 最后再加 `pid` 和完整保护逻辑

---

## 6. 现阶段的最终建议

结合你当前工程状态，我给出的结论是：

- 你的 CubeMX 工程已经具备复刻基础，但还不是“可以直接拷代码”的状态
- 原工程的 `_cb.c` 文件不要照搬，应统一合并为 HAL 回调风格
- 在继续新增电机驱动文件前，应该先完成一轮基础硬件链路测试
- 最稳妥的路线是：**先测基础、再加底层 BSP、再加 Hall/ADC、最后上换相和闭环**

---

## 7. 我建议你下一步优先做什么

最推荐的下一步只有两个：

1. 先让我帮你基于当前工程写一个“Phase 0 基础自检版 `main.c` 测试程序”
2. 然后我再帮你输出第一批要新增的 `bsp_ctrl_sd` / `bsp_pwm` / `bsp_hall` / `bsp_adc_motor` 文件骨架

如果你愿意，我下一步可以直接继续帮你做这两件事，并且按你现在这个工程目录直接给出可落地代码。
