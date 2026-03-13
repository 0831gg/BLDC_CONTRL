# 1BLDC Hall 复刻评估与实施建议

> 目标：把 `D:\Github DownLoad\motorcontrol\bldc_hall_f407` 的裸机/标准库 BLDC Hall 控制代码，迁移到 `D:\Electrical Design\MOTOR\My_Motor_Control_Project\my_control` 这个 CubeMX + HAL 工程。

---

## 1. 现有 CubeMX 配置能否支撑复刻

先说结论：

- 你当前的 CubeMX 工程已经具备了 **六步有感 BLDC 复刻的大部分硬件基础**。
- 但还 **不能直接无修改复刻原工程**，有几项关键配置仍需补齐或确认。
- 尤其是 **ADC 触发链路、TIM1 触发源策略、Break 极性、Hall 上下拉、电机测试顺序**，这些会直接影响后续驱动文件能否稳定落地。

### 1.1 已具备的资源

结合 `BLDC_HALL.ioc`、`Core/Src/*.c` 和当前用户层文件，已经具备如下条件：

| 模块 | 当前状态 | 说明 |
|------|----------|------|
| TIM1 三路 PWM | 已具备 | `PA8/PA9/PA10 -> TIM1_CH1/2/3`，适合三相上桥 PWM |
| TIM1 Break 输入 | 已具备 | `PB12 -> TIM1_BKIN`，已经开中断 |
| Hall 三路输入 | 已具备 | `PA0/PA1/PA2`，双边沿 EXTI |
| 下桥控制 GPIO | 已具备 | `PB13/PB14/PB15` 当前是普通输出，符合六步控制需要 |
| 驱动器使能脚 | 已具备 | `PC13 -> PWM_Enable` |
| ADC1 规则组 + DMA | 已具备 | 当前采 `VBUS`、`TEMP` |
| ADC2 注入组 | 已具备 | 当前采三相电流 `AMP_U/V/W` |
| TIM5 自由运行计时 | 已具备 | 可用于 Hall 时间戳测速 |
| USART1 调试串口 | 已具备 | 已能跑 `test_phase0` |
| LED / KEY / DWT 延时 | 已具备 | 用户层基础测试已完成 |

这些资源说明：**从外设种类和引脚分配层面，当前工程是可以作为复刻底座的。**

### 1.2 当前仍缺少或需要修正的配置

下面这些是正式开始移植前，建议优先处理的项目。

| 项目 | 当前情况 | 建议 |
|------|----------|------|
| ADC2 注入触发源 | 仍是 `ADC_INJECTED_SOFTWARE_START` | 改为 `TIM1 Trigger Out` 上升沿触发 |
| TIM1 触发输出策略 | 当前 `TRGO=UPDATE`，CH4 为 `OC Timing` | 明确后续统一采用 `TRGO_UPDATE` 还是 `OC4REF` |
| Break 极性 | 当前为高电平触发 | 必须结合实际过流保护电路确认 |
| Break 输入 GPIO 模式 | `PB12` 当前在 `tim.c` 里被配置成 `GPIO_MODE_AF_OD` + `NOPULL` | 建议复查原理图，通常更常见的是 AF 输入/上拉逻辑 |
| Hall 输入上下拉 | 当前 `GPIO_NOPULL` | 如果霍尔输出是开漏/集电极开路，建议改上拉 |
| ADC1 规则通道数量 | 当前只有 `VBUS`、`TEMP` 两路 | 若你决定用按键调速，则可以不补第三路速度 ADC |
| ADC2 注入结果启动流程 | 当前只有初始化，没有看到用户层启动注入转换 | 后续 BSP 驱动里必须补 `Calibration + Start_Injected` |
| TIM5 使用方式 | 已初始化，但主程序未启动 | 后续 Hall 驱动初始化中应 `HAL_TIM_Base_Start(&htim5)` |
| PWM 实际启动流程 | `main.c` 当前只跑 `Test_Phase0_*` | 后续要单独做 Phase1 PWM 自检入口 |

### 1.2.1 需要修正的配置清单（按顺序执行）

#### 1. ADC2 注入触发源

- 当前情况：`ADC2` 注入组仍是 `ADC_INJECTED_SOFTWARE_START`
- 建议修改：改为 `TIM1 Trigger Out event`，触发边沿选上升沿
- 原因：三相电流采样需要和 PWM 周期同步，否则后续电流观测、保护判断、闭环控制的时序会不稳定
- 当前复查结果：已配置正确。当前生成代码为 `ADC_EXTERNALTRIGINJEC_T1_CC4 + RISING`，已经实现与 `TIM1 CH4/OC4REF` 同步触发，满足移植所需的同步采样条件。

#### 2. TIM1 触发输出策略

- 当前情况：`TIM1 TRGO = UPDATE`，`CH4` 配置为 `OC Timing`
- 建议修改：明确后续统一采用哪一种方案
  - 方案 A：保留 `TRGO_UPDATE`
  - 方案 B：改为 `OC4REF` 作为 ADC 触发参考
- 原因：后面 `ADC2` 注入采样、PWM 同步采样点、调试波形分析都依赖这一触发基准，不能一边写驱动一边改策略
- 当前复查结果：已配置正确。现在 `TIM1 TRGO=OC4REF`，`ADC2` 触发源为 `TIM1_CC4`，两者已经统一到同一套采样基准，可直接作为后续注入采样触发方案。

#### 3. Break 极性

- 当前情况：`TIM1 BKIN` 现在按高电平触发配置
- 建议修改：结合驱动板原理图和故障脚实际输出逻辑，确认到底应为高有效还是低有效
- 原因：如果极性配反，轻则误触发停机，重则真正故障时不能及时关断
- 当前复查结果：代码配置已正确。`.ioc` 和 `tim.c` 中都已改为 `TIM_BREAKPOLARITY_LOW`，并且 `SourceBRKDigInputPolarity` 也为低有效，和当前预期的低电平故障触发逻辑一致；上电前仍建议再结合原理图或示波器确认一次。

#### 4. Break 输入引脚模式

- 当前情况：`PB12` 在当前生成代码里是 `GPIO_MODE_AF_OD`，`GPIO_NOPULL`
- 建议修改：复查是否符合实际硬件连接方式，必要时调整为更合适的复用输入/上下拉配置
- 原因：Break 是保护链路，输入模式不对会导致悬空、误触发、保护失效等问题
- 当前复查结果：基本满足移植条件。`PB12` 已恢复为 `TIM1_BKIN`，`BreakState = TIM_BREAK_ENABLE`，`HAL_TIMEx_ConfigBreakInput()` 也已开启 BKIN 输入链路；不过当前 GPIO 仍是 CubeMX 生成的 `GPIO_MODE_AF_OD + GPIO_NOPULL`，这点建议后续结合硬件再确认是否需要改成更适合的输入形式。

#### 5. Hall 输入上下拉

- 当前情况：`PA0/PA1/PA2` 配置为 EXTI 双边沿，但内部上下拉是 `GPIO_NOPULL`
- 建议修改：根据霍尔传感器输出类型确认是否需要改成上拉
- 原因：如果霍尔输出是开漏或集电极开路，当前无上下拉配置会导致输入状态不稳定
- 当前复查结果：已配置正确。`.ioc` 和 `gpio.c` 中 `PA0/PA1/PA2` 都已经改为 `GPIO_PULLUP`。

#### 6. ADC2 注入转换启动流程

- 当前情况：CubeMX 已初始化 `ADC2` 注入组，但当前工程里还没有看到完整的 `校准 + 启动注入转换` 用户层流程
- 建议修改：后续在 `bsp_adc_motor.c` 中明确补上
  - `HAL_ADCEx_Calibration_Start()`
  - `HAL_ADCEx_InjectedStart_IT()` 或与当前方案对应的启动方式
- 原因：只配置外设但不完成启动流程，后面测试时会出现“配置看着对，但数据始终不更新”的问题
- 当前复查结果：未满足。当前 `main.c` 仍只做外设初始化并进入 `test_phase0`，没有看到 `ADC2` 校准和注入转换启动流程；这项仍需要在后续 `bsp_adc_motor.c` 或 `test_phase2` 中补上。

#### 7. TIM5 启动方式

- 当前情况：`TIM5` 已初始化为自由运行定时器，但主程序中还没有正式启动
- 建议修改：在后续 `bsp_hall_init()` 中调用 `HAL_TIM_Base_Start(&htim5)`
- 原因：Hall 测速依赖时间戳，若 TIM5 没启动，后续读到的时间差无意义
- 当前复查结果：未满足。`MX_TIM5_Init()` 已执行，但 `main.c` 中没有看到 `HAL_TIM_Base_Start(&htim5)` 或等效启动代码；这项仍需要在后续 `bsp_hall_init()` 或测试入口中补上。

#### 8. PWM_Enable 极性确认

- 当前情况：`PC13` 当前默认输出低电平，处于安全禁止态
- 建议修改：后续在 `bsp_ctrl_sd` 中明确采用以下语义
  - `PC13=0`：半桥芯片不工作 / 禁止输出
  - `PC13=1`：半桥芯片允许工作 / 使能输出
- 原因：如果使能极性写反，Phase1 虽然可能测到 PWM 波形，但驱动器真实工作状态会与软件状态相反
- 当前复查结果：已确认极性。根据原理图，`PC13 = 低电平` 时半桥芯片不工作，因此该脚应按“低电平禁止/高电平使能”处理。当前 `.ioc` 和 `gpio.c` 中默认输出 `GPIO_PIN_RESET` 属于安全默认态，后续 `bsp_ctrl_sd_enable()` / `bsp_ctrl_sd_disable()` 必须按这个极性实现。

#### 9. PWM 实际启动验证入口

- 当前情况：`main.c` 现在只跑 `test_phase0`
- 建议修改：后续新增 `test_phase1` 作为独立入口，不要直接把 PWM、电流采样、Hall、换相逻辑一起挂进主循环
- 原因：分阶段验证可以把问题范围压缩在单一模块，避免多个驱动同时引入后难以定位错误
- 当前复查结果：已满足。`main.c` 已切换为独立的 `Test_Phase1_Init()` 和 `Test_Phase1_Loop()` 入口，`Phase1` 已可单独完成 PWM / Enable / 下桥输出验证。

#### 10. ADC1 规则通道数量是否需要补第三路

- 当前情况：`ADC1` 当前只采 `VBUS` 和 `TEMP`
- 建议修改：先明确速度给定方式
  - 若采用按键调速：保持 2 路即可
  - 若仍要保留电位器调速：再补第三路 ADC 通道
- 原因：这会影响后续 `analog_calculate`、`motor_execute`、调速输入接口的设计，不宜后期反复变动
- 当前复查结果：可用于当前移植方案。`ADC1` 仍保持 2 路规则采样，这在“按键调速”方案下是正确的；如果后面要恢复电位器调速，再补第三路即可。

### 1.3 现阶段可以不强行复刻的内容

原工程中有些模块不建议一比一照搬：

- `bsp_systick.c`：HAL 已有 `HAL_GetTick()`，当前足够支撑阶段性测试。
- `bsp_io.c`：CubeMX 已生成 `MX_GPIO_Init()`，不必保留独立 IO 初始化模块。
- `bsp_dac.c`：你当前已有串口 + Vofa，更适合做调试输出。
- 原工程速度旋钮 ADC：你当前工程更适合先走 **按键调速** 路线。
- 原工程 F407 特定中断/引脚补丁：例如 `PH10/PH2` 的 EXTI bug 规避逻辑，在 G474 上不需要照搬。

### 1.4 结论

当前 CubeMX 配置的判断是：

- **可以作为复刻起点**
- **不建议现在就直接搬整套控制代码进去**
- **应先完成最小闭环验证链路，再逐步加入驱动文件**

在继续之前，建议优先完成以下最小修改：

1. 把 ADC2 注入触发改成 `TIM1 Trigger Out event`。
2. 确认 Hall 输入是否需要内部上拉。
3. 确认 Break 极性与驱动器故障脚真实逻辑一致。
4. 明确 TIM1 的 ADC 同步策略是否统一采用 `TRGO_UPDATE`。
5. 在用户层准备独立的 `test_phase1 / test_phase2 / test_phase3` 入口，不要直接跳到电机闭环。

---

## 2. 原工程回调函数在 HAL 工程中应如何处理

原工程把很多逻辑拆成：

- `bsp_xxx.c`
- `bsp_xxx_cb.c`

这种拆法在标准库项目里常见，但在 CubeMX + HAL 工程里，**不建议继续保留 `_cb.c` 这种结构**。更合适的做法是：

- 中断入口保留在 `Core/Src/stm32g4xx_it.c`
- 中断入口里调用 `HAL_XXX_IRQHandler(...)`
- 业务处理放到 HAL 弱回调中
- 回调实现合并进对应 BSP 文件

### 2.1 推荐的合并方式

| 原工程文件 | 在新工程中的建议归属 | HAL 对应方式 |
|------------|----------------------|--------------|
| `bsp_hall_exti_cb.c` | 合并进 `bsp_hall.c` | `HAL_GPIO_EXTI_Callback()` |
| `bsp_hall_speed_cb.c` | 不单独保留 | 用 TIM5 自由运行时间戳替代 |
| `bsp_pwm_cb.c` | 合并进 `bsp_pwm.c` | `HAL_TIMEx_BreakCallback()` |
| `bsp_adc_cb.c` | 合并进 `bsp_adc_motor.c` | `HAL_ADCEx_InjectedConvCpltCallback()` |
| `bsp_uart_cb.c` | 合并进 `bsp_uart.c` | `HAL_UART_RxCpltCallback()`，若后续需要 RX |
| `motor_sensor.c` | 拆分合并 | Hall 读取进 `bsp_hall.c`，换相逻辑进 `motor_ctrl.c` |

### 2.2 各类回调应该怎么改

#### Hall 中断

原工程本质逻辑是：任意 Hall 变化 -> 调换相函数。

HAL 下建议直接写：

```c
void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin)
{
    if ((GPIO_Pin == HALL_U_Pin) ||
        (GPIO_Pin == HALL_V_Pin) ||
        (GPIO_Pin == HALL_W_Pin))
    {
        bsp_hall_update_timestamp();
        motor_sensor_mode_phase();
    }
}
```

也就是说：

- 不再保留 `hall_uvw_irq_cb[]`
- 不再为 U/V/W 单独写三层转发回调
- 中断里只做“更新时间戳 + 触发换相/记录状态”

#### Hall 速度回调

原工程通过定时器高频中断累加软件计数。

你当前 G474 工程里 `TIM5` 已经是 32 位自由运行定时器，这时更推荐：

- 不开高频中断
- 直接在 Hall 跳变时读 `TIM5->CNT`
- 用相邻时间戳差值计算速度

这意味着：

- `bsp_hall_speed_cb.c` 可以直接取消
- 速度逻辑放进 `bsp_hall.c`

#### PWM Break 回调

原工程 `bsp_pwm_cb.c` 基本只是一个中断占位。

HAL 下更合理的处理是：

```c
void HAL_TIMEx_BreakCallback(TIM_HandleTypeDef *htim)
{
    if (htim->Instance == TIM1)
    {
        bsp_pwm_all_close();
        bsp_ctrl_sd_disable();
        motor_ctrl_set_fault(MOTOR_FAULT_BREAK);
    }
}
```

这个实现建议放进 `User/Bsp/Src/bsp_pwm.c`。

#### ADC 注入完成回调

原工程 `bsp_adc_cb.c` 做了两类事情：

1. 读注入结果
2. 在注入回调中手工触发规则组转换

到了你现在的 HAL 工程，建议分开：

- `ADC1` 规则组已经是 DMA 连续采样，直接读缓冲区
- `ADC2` 注入组在回调里只负责更新电流缓存
- **不要继续照搬“注入回调里再软件启动规则组”这条链路**

所以 `bsp_adc_cb.c` 的处理方式应是：

- 保留“读注入结果”
- 删除“手工再触发 ADC1 规则组”

#### UART 回调

你当前 `User/Bsp/Src/bsp_uart.c` 还是阻塞发送风格，前期完全够用。

因此当前建议是：

- Phase0~Phase3 不必引入 `HAL_UART_RxCpltCallback()`
- 如果后面要做串口命令调速，再补 RX 中断/环形缓冲区

### 2.3 建议删除或不再创建的文件

结合你现在工程结构，后续建议：

| 文件/类型 | 处理建议 | 原因 |
|-----------|----------|------|
| `*_cb.c` 独立回调文件 | 不再新增 | HAL 回调更适合直接放入对应模块 |
| `bsp_hall_speed_cb.c` | 不复刻 | TIM5 自由运行时间戳方案更简单 |
| `bsp_systick.c` | 不复刻 | HAL 时间基已具备 |
| `bsp_io.c` | 不复刻 | CubeMX 已接管 GPIO 初始化 |
| `bsp_dac.c` | 可不复刻 | 你已有 Vofa 串口链路 |
| `motor_sensor.c` 整体照搬 | 不建议 | 需拆成 Hall 采样与换相控制两部分 |

---

## 3. 如果想逐步测试地添加驱动文件，应该怎样加

这里最重要的原则是：

- **每加一层驱动，只验证一层功能**
- **不跨阶段叠加问题**
- **没有通过上一阶段，就不要进下一阶段**

### 3.1 推荐的添加顺序

#### Phase 0：基础链路

当前已完成：

- `bsp_led`
- `bsp_key`
- `bsp_uart`
- `bsp_delay`
- `vofa_*`
- `test_phase0`

这一阶段的目标是确认：

- 时钟没问题
- 主循环正常
- 串口/Vofa 可用
- 按键可作为后续调试输入

你这一步已经具备进入下一阶段的条件。

##### Phase 0 需要构建的函数

| 函数 | 功能 | HAL / 当前实现 |
|------|------|----------------|
| `void Test_Phase0_Init(void)` | 初始化 Phase0 全部基础模块 | 调用 `Delay_Init()`、`BSP_UART_Init()`、`LED_Init()`、`Key_Init()`、`VOFA_Init()` |
| `void Test_Phase0_Loop(void)` | 主循环基础验证入口 | 调用 `HAL_GetTick()` 做周期调度 |
| `uint32_t Delay_Init(void)` | 初始化 DWT 微秒延时 | 使用 `CoreDebug->DEMCR`、`DWT->CTRL`、`DWT->CYCCNT` |
| `void Delay_us(uint32_t us)` | 微秒延时 | 基于 `DWT->CYCCNT` 轮询 |
| `void Delay_ms(uint32_t ms)` | 毫秒延时 | 基于 `Delay_us()` 累加 |
| `void BSP_UART_Init(void)` | 串口基础初始化封装 | 依赖 CubeMX 生成的 `MX_USART1_UART_Init()` |
| `void BSP_UART_Printf(const char *fmt, ...)` | 串口格式化输出 | `HAL_UART_Transmit(&huart1, ...)` |
| `void LED_Init(void)` | 初始化 LED 默认状态 | `HAL_GPIO_WritePin()` |
| `void LED_Toggle(led_enum_e num)` | LED 翻转 | `HAL_GPIO_TogglePin()` |
| `void Key_Init(KeyGroup_HandleTypeDef *hkey, uint16_t long_press_time)` | 按键参数初始化 | 依赖 `HAL_GPIO_ReadPin()` 读取引脚 |
| `void Key_Scan(KeyGroup_HandleTypeDef *hkey)` | 按键扫描 | `HAL_GPIO_ReadPin()` + 软件状态机 |
| `void VOFA_Init(void)` | 初始化 Vofa 输出链路 | 依赖串口发送接口 |
| `void VOFA_JustFloat(float *data, uint16_t num)` | 发送 JustFloat 波形帧 | 最终走 `HAL_UART_Transmit()` 或 DMA 发送 |

##### Phase 0 HAL 替换说明

| 原裸机/标准库思路 | HAL / 当前工程替换 |
|------------------|---------------------|
| `USART_SendData()` | `HAL_UART_Transmit()` |
| GPIO 置位/复位 | `HAL_GPIO_WritePin()` |
| GPIO 翻转 | `HAL_GPIO_TogglePin()` |
| 轮询滴答/自写 systick | `HAL_GetTick()` |
| 裸机延时循环 | `DWT->CYCCNT` 精确延时 |

#### Phase 1：先加功率输出底层，不碰换相逻辑

建议先新增：

- `User/Bsp/Inc/bsp_ctrl_sd.h`
- `User/Bsp/Src/bsp_ctrl_sd.c`
- `User/Bsp/Inc/bsp_pwm.h`
- `User/Bsp/Src/bsp_pwm.c`
- `User/Test/Inc/test_phase1.h`
- `User/Test/Src/test_phase1.c`

这一阶段只验证：

- `PC13` 能否正确禁止/使能驱动器（低=禁止，高=使能）
- `PA8/PA9/PA10` 是否是 10kHz PWM
- `PB13/PB14/PB15` 是否能独立翻转
- `Break` 触发后是否能安全关断

**这一步不接电机功率，不做闭环，不做 Hall 换相。**

##### Phase 1 需要构建的函数

###### `bsp_ctrl_sd`

| 函数 | 功能 | HAL 替换 |
|------|------|----------|
| `void bsp_ctrl_sd_init(void)` | 初始化驱动使能脚并保持安全态 | 依赖 CubeMX 的 `MX_GPIO_Init()`，再调用 `bsp_ctrl_sd_disable()` |
| `void bsp_ctrl_sd_enable(void)` | 使能半桥输出 | `HAL_GPIO_WritePin(GPIOC, GPIO_PIN_13, GPIO_PIN_SET)` |
| `void bsp_ctrl_sd_disable(void)` | 禁止半桥输出 | `HAL_GPIO_WritePin(GPIOC, GPIO_PIN_13, GPIO_PIN_RESET)` |

###### `bsp_pwm`

| 函数 | 功能 | HAL 替换 |
|------|------|----------|
| `void bsp_pwm_init(void)` | 初始化 PWM 模块、默认占空比为 0 | `__HAL_TIM_SET_COMPARE(&htim1, TIM_CHANNEL_x, 0)` |
| `void bsp_pwm_duty_set(uint16_t duty)` | 统一设置三相占空比 | `__HAL_TIM_SET_COMPARE(&htim1, TIM_CHANNEL_1/2/3, duty)` |
| `void bsp_pwm_all_open(void)` | 打开三路 PWM 主输出 | `HAL_TIM_PWM_Start()` + `__HAL_TIM_MOE_ENABLE()` |
| `void bsp_pwm_all_close(void)` | 关闭三路 PWM 输出 | `HAL_TIM_PWM_Stop()` |
| `void bsp_pwm_ctrl_single_phase(uint8_t phase)` | 仅打开指定相上桥 PWM | `TIM1->CCER` 位操作或 HAL 宏 |
| `void bsp_pwm_lower_set(uint8_t un, uint8_t vn, uint8_t wn)` | 控制 PB13/PB14/PB15 下桥 | `HAL_GPIO_WritePin()` |
| `void bsp_pwm_lower_all_off(void)` | 关闭全部下桥 | `HAL_GPIO_WritePin(GPIOB, ..., GPIO_PIN_RESET)` |
| `void HAL_TIMEx_BreakCallback(TIM_HandleTypeDef *htim)` | Break 触发后硬停机 | 调 `bsp_pwm_all_close()` + `bsp_ctrl_sd_disable()` |

###### `test_phase1`

| 函数 | 功能 | HAL / 依赖 |
|------|------|------------|
| `void Test_Phase1_Init(void)` | 初始化驱动使能和 PWM 测试环境 | 调 `bsp_ctrl_sd_init()`、`bsp_pwm_init()` |
| `void Test_Phase1_Loop(void)` | 响应按键命令、更新 LED 指示、处理 Break 事件 | 调 `HAL_GetTick()`、`Key_Scan()` |

###### `test_phase1` 按键与 Step 规则

| 项目 | 规则 |
|------|------|
| `KEY0` | 切到下一步 |
| `KEY1` | 切到上一步 |
| `KEY2` | 强制进入 `Step5 All outputs off` 安全态 |

| Step | 含义 | LED 显示 |
|------|------|----------|
| `Step0` | `SD disable, PWM off` | `LED0 OFF`，`LED1 OFF` |
| `Step1` | `SD enable only` | `LED0 ON`，`LED1 OFF` |
| `Step2` | `CH1/CH2/CH3 50% PWM (10kHz)` | `LED0 OFF`，`LED1 ON` |
| `Step3` | `U phase PWM only` | `LED0 ON`，`LED1 ON` |
| `Step4` | `Lower UN/WN on` | `LED0` 闪烁，`LED1 OFF` |
| `Step5` | `All outputs off` | `LED0 OFF`，`LED1` 闪烁 |

说明：

- `Step0` 和 `Step5` 都属于安全态，但用途不同。
- `Step0` 是默认进入的初始态，用于确认上电后驱动默认关闭。
- `Step5` 是人工测试结束或紧急退出时的全关断态，可由 `KEY2` 直接进入。

###### `test_phase1` 当前代码已落地内容

当前 `User/Test/Src/test_phase1.c` 已经按下面规则实现：

- `main.c` 已切换为调用 `Test_Phase1_Init()` 和 `Test_Phase1_Loop()`
- `Test_Phase1_Init()` 会完成：
  - `BSP_UART_Init()`
  - `LED_Init()`
  - `Key_Init(&s_key, KEY_DEFAULT_LONG_PRESS_TIME)`
  - `bsp_ctrl_sd_init()`
  - `bsp_pwm_init()`
- 初始化完成后默认进入 `Step0`
- `Test_Phase1_Loop()` 已实现：
  - 每 `20ms` 扫描一次按键
  - `KEY0` 下一步，`KEY1` 上一步，`KEY2` 强制进入 `Step5`
  - Break 事件检测与串口提示
  - LED 步骤指示刷新

当前代码中的 Step 行为如下：

| Step | 当前代码行为 |
|------|--------------|
| `Step0` | `bsp_ctrl_sd_disable()`，PWM 全关，下桥全关 |
| `Step1` | `bsp_ctrl_sd_enable()`，不输出 PWM |
| `Step2` | 使能驱动，三路 PWM 输出 50% 占空比（10kHz） |
| `Step3` | 使能驱动，仅 U 相输出 50% PWM（10kHz） |
| `Step4` | 禁止驱动，仅下桥 `UN/WN` 输出有效 |
| `Step5` | 禁止驱动，PWM=0，下桥全关 |

对应到当前板级引脚，`test_phase1` 每个 Step 的功率输出状态如下：

| Step | `PC13` 驱动使能 | `PA8 / TIM1_CH1` | `PA9 / TIM1_CH2` | `PA10 / TIM1_CH3` | `PB13 / UN` | `PB14 / VN` | `PB15 / WN` |
|------|------------------|------------------|------------------|-------------------|--------------|--------------|--------------|
| `Step0` | 低，驱动禁止 | 无 PWM | 无 PWM | 无 PWM | 低，关闭 | 低，关闭 | 低，关闭 |
| `Step1` | 高，驱动使能 | 无 PWM | 无 PWM | 无 PWM | 低，关闭 | 低，关闭 | 低，关闭 |
| `Step2` | 高，驱动使能 | 50% PWM，10kHz | 50% PWM，10kHz | 50% PWM，10kHz | 低，关闭 | 低，关闭 | 低，关闭 |
| `Step3` | 高，驱动使能 | 50% PWM，10kHz | 无 PWM | 无 PWM | 低，关闭 | 低，关闭 | 低，关闭 |
| `Step4` | 高，驱动使能 | 无 PWM | 无 PWM | 无 PWM | 高，导通 | 低，关闭 | 高，导通 |
| `Step5` | 低，驱动禁止 | 无 PWM | 无 PWM | 无 PWM | 低，关闭 | 低，关闭 | 低，关闭 |

补充说明：

- 上桥 PWM 输出引脚固定为 `PA8/PA9/PA10`，分别对应 `TIM1_CH1/CH2/CH3`。
- 下桥数字控制引脚固定为 `PB13/PB14/PB15`，分别对应 `UN/VN/WN`。
- Break 输入为 `PB12 / TIM1_BKIN`，当前按低电平故障触发配置，不属于 Step 主动输出引脚，但会直接切断 PWM。
- `Step4` 代码实际调用的是 `bsp_pwm_lower_set(1U, 0U, 1U)`，所以当前只有 `PB13` 和 `PB15` 拉高，`PB14` 保持低电平。

当前代码中的 LED 规则如下：

| Step | 当前 LED 行为 |
|------|----------------|
| `Step0` | `LED0 OFF`，`LED1 OFF` |
| `Step1` | `LED0 ON`，`LED1 OFF` |
| `Step2` | `LED0 OFF`，`LED1 ON` |
| `Step3` | `LED0 ON`，`LED1 ON` |
| `Step4` | `LED0` 以 `200ms` 周期闪烁，`LED1 OFF` |
| `Step5` | `LED0 OFF`，`LED1` 以 `200ms` 周期闪烁 |

对应代码文件：

- `Core/Src/main.c`
- `User/Test/Src/test_phase1.c`
- `User/Bsp/Src/bsp_ctrl_sd.c`
- `User/Bsp/Src/bsp_pwm.c`

###### `Phase1` 完成情况

当前 `Phase1` 已完成，并已通过板级静态输出验证。

- `Step1` 到 `Step5` 的引脚行为与代码设计一致。
- `PA8 / TIM1_CH1`、`PA9 / TIM1_CH2`、`PA10 / TIM1_CH3` 的 PWM 波形已通过示波器确认正确。
- `PC13` 驱动使能、`PB13/PB14/PB15` 下桥控制、`PB12 / TIM1_BKIN` 低电平 Break 保护链路当前配置与测试目标一致。
- 这说明 `TIM1` PWM 输出、驱动使能控制、下桥 GPIO 控制、Phase1 的 Step 切换逻辑都已经打通。
- 因此当前项目状态可以从 `Phase1` 进入下一阶段，后续重点应转向 `Phase2` 的 ADC 采样链路验证。

##### Phase 1 HAL 替换说明

| 原裸机/标准库思路 | HAL / 当前工程替换 |
|------------------|---------------------|
| `GPIO_WriteBit()` 控制使能脚 | `HAL_GPIO_WritePin()` |
| 直接写 `TIM1->CCR1/2/3` | `__HAL_TIM_SET_COMPARE()` |
| `TIM_CCxCmd()` 单相开关 | `TIM1->CCER` 位操作 |
| `TIM_CtrlPWMOutputs()` | `__HAL_TIM_MOE_ENABLE()` |
| Break 中断分立回调文件 | `HAL_TIMEx_BreakCallback()` 合并入 `bsp_pwm.c` |

#### Phase 2：加 ADC 采样驱动

建议新增：

- `User/Bsp/Inc/bsp_adc_motor.h`
- `User/Bsp/Src/bsp_adc_motor.c`
- `User/Test/Inc/test_phase2.h`
- `User/Test/Src/test_phase2.c`

这一阶段只验证：

- `VBUS` 原始值与万用表是否一致
- `TEMP` 是否随温度变化
- `ADC2` 注入三相电流是否能随 TIM1 同步更新

注意：**进入这一步前，先把 ADC2 注入触发改成 TIM1 触发。**

##### Phase 2 需要构建的函数

| 函数 | 功能 | HAL 替换 |
|------|------|----------|
| `void bsp_adc_init(void)` | 启动 ADC1 DMA、ADC2 校准、ADC2 注入转换 | `HAL_ADC_Start_DMA()` + `HAL_ADCEx_Calibration_Start()` + `HAL_ADCEx_InjectedStart_IT()` |
| `uint16_t bsp_adc_get_vbus_raw(void)` | 获取 VBUS 原始 ADC 值 | 读取 `adc1_dma_buf[0]` |
| `uint16_t bsp_adc_get_temp_raw(void)` | 获取温度原始 ADC 值 | 读取 `adc1_dma_buf[1]` |
| `float bsp_adc_get_vbus(void)` | 换算母线电压 | 软件换算 |
| `float bsp_adc_get_temp(void)` | 换算温度 | 软件换算 |
| `void bsp_adc_get_phase_current(int16_t *iu, int16_t *iv, int16_t *iw)` | 读取三相电流注入值 | `HAL_ADCEx_InjectedGetValue(&hadc2, rank)` |
| `void HAL_ADCEx_InjectedConvCpltCallback(ADC_HandleTypeDef *hadc)` | 注入转换完成回调 | 在回调中读取 `hadc2` 注入结果 |
| `void Test_Phase2_Init(void)` | 初始化 ADC 测试环境 | 调 `bsp_adc_init()` |
| `void Test_Phase2_Loop(void)` | 周期打印/上传 VBUS、TEMP、电流值 | 调 `HAL_GetTick()` 节拍发送 |

##### Phase 2 HAL 替换说明

| 原裸机/标准库思路 | HAL / 当前工程替换 |
|------------------|---------------------|
| 手工启动规则 ADC | `HAL_ADC_Start_DMA()` |
| `ADC_GetInjectedConversionValue()` | `HAL_ADCEx_InjectedGetValue()` |
| 注入完成中断单独转发 | `HAL_ADCEx_InjectedConvCpltCallback()` |
| 注入回调里再触发规则组 | 直接读取 ADC1 DMA 缓冲区，不再软件链式触发 |

#### Phase 3：加 Hall 采样与测速

建议新增：

- `User/Bsp/Inc/bsp_hall.h`
- `User/Bsp/Src/bsp_hall.c`
- `User/Test/Inc/test_phase3.h`
- `User/Test/Src/test_phase3.c`

只验证：

- Hall 三位状态是否正确
- 顺逆时针状态序列是否合理
- TIM5 时间戳差值是否稳定
- 速度计算是否基本可信

**这一步仍然不需要驱动电机上电运行。**

##### Phase 3 需要构建的函数

| 函数 | 功能 | HAL 替换 |
|------|------|----------|
| `void bsp_hall_init(void)` | 启动 TIM5 自由运行测速基准 | `HAL_TIM_Base_Start(&htim5)` |
| `uint8_t bsp_hall_get_state(void)` | 读取 Hall 三位状态 | `HAL_GPIO_ReadPin(GPIOA, HALL_U/V/W_Pin)` |
| `uint32_t bsp_hall_get_speed(void)` | 返回 RPM | 基于 `TIM5->CNT` 差值计算 |
| `uint32_t bsp_hall_get_commutation_time(void)` | 返回最近换相时间戳 | `__HAL_TIM_GET_COUNTER(&htim5)` |
| `void bsp_hall_irq_enable(void)` | 使能 Hall 中断 | `HAL_NVIC_EnableIRQ()` |
| `void bsp_hall_irq_disable(void)` | 关闭 Hall 中断 | `HAL_NVIC_DisableIRQ()` |
| `void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin)` | Hall 跳变回调 | 读取 Hall 状态 + 时间戳 |
| `void Test_Phase3_Init(void)` | 初始化 Hall 测试环境 | 调 `bsp_hall_init()` |
| `void Test_Phase3_Loop(void)` | 输出 Hall 状态、速度、时间差 | 调 `HAL_GetTick()` 节拍打印 |

##### Phase 3 HAL 替换说明

| 原裸机/标准库思路 | HAL / 当前工程替换 |
|------------------|---------------------|
| `GPIO_ReadInputDataBit()` 读霍尔 | `HAL_GPIO_ReadPin()` |
| TIM4 10us 中断累加 | TIM5 32 位自由运行计数 |
| EXTI 分立回调文件 | `HAL_GPIO_EXTI_Callback()` |
| 软件时间计数变量 | `__HAL_TIM_GET_COUNTER(&htim5)` |

#### Phase 4：加六步换相表与启动/停止控制

建议新增：

- `User/Motor/Inc/motor_phase_tab.h`
- `User/Motor/Src/motor_phase_tab.c`
- `User/Motor/Inc/motor_ctrl.h`
- `User/Motor/Src/motor_ctrl.c`
- `User/Test/Inc/test_phase4.h`
- `User/Test/Src/test_phase4.c`

只验证：

- 读当前 Hall 值后能映射出正确导通相
- 空载/限压条件下能完成低速启动
- 停机后上桥和下桥都关断

##### Phase 4 需要构建的函数

###### `motor_phase_tab`

| 函数 | 功能 | HAL / 依赖 |
|------|------|------------|
| `void mos_up_vn(uint16_t duty)` | U+ V- 换相 | 调 `bsp_pwm_ctrl_single_phase(U_PHASE)` + 下桥 GPIO |
| `void mos_up_wn(uint16_t duty)` | U+ W- 换相 | 同上 |
| `void mos_vp_un(uint16_t duty)` | V+ U- 换相 | 同上 |
| `void mos_vp_wn(uint16_t duty)` | V+ W- 换相 | 同上 |
| `void mos_wp_un(uint16_t duty)` | W+ U- 换相 | 同上 |
| `void mos_wp_vn(uint16_t duty)` | W+ V- 换相 | 同上 |

###### `motor_ctrl`

| 函数 | 功能 | HAL / 依赖 |
|------|------|------------|
| `void motor_ctrl_init(void)` | 初始化控制参数和状态 | 软件初始化 |
| `int motor_start(uint16_t duty, uint8_t direction)` | 电机启动流程 | 调 `bsp_ctrl_sd_enable()`、`bsp_pwm_lower_set()`、`Delay_ms()`、`bsp_hall_get_state()` |
| `void motor_stop(void)` | 电机停止流程 | 调 `bsp_hall_irq_disable()`、`bsp_pwm_all_close()`、`bsp_ctrl_sd_disable()` |
| `void motor_sensor_mode_phase(void)` | Hall 到六步换相执行 | 调 `bsp_hall_get_state()` + `mos_xp_yn()` |
| `void motor_ctrl_set_fault(uint8_t fault)` | 记录故障状态 | 软件状态更新 |
| `void Test_Phase4_Init(void)` | 初始化开环测试环境 | 调 `motor_ctrl_init()` |
| `void Test_Phase4_Loop(void)` | 启停/方向/占空比测试 | 调 `motor_start()`、`motor_stop()` |

##### Phase 4 HAL 替换说明

| 原裸机/标准库思路 | HAL / 当前工程替换 |
|------------------|---------------------|
| 相位表直接写 GPIO/TIM 寄存器 | 通过 `bsp_pwm_*` 和 `HAL_GPIO_WritePin()` 封装 |
| 启动时直接控制桥臂 | 调 `bsp_ctrl_sd_enable()`、`bsp_pwm_lower_set()` |
| Hall 中断中直接查表换相 | `HAL_GPIO_EXTI_Callback()` -> `motor_sensor_mode_phase()` |

#### Phase 5：再加状态机与保护

最后再考虑：

- `motor_execute.c`
- `analog_calculate.c`
- `pid.c`（如果要闭环）

也就是说，**保护、调速、状态机、闭环** 全都应晚于“基础驱动已验证”。

##### Phase 5 需要构建的函数

###### `motor_execute`

| 函数 | 功能 | HAL / 依赖 |
|------|------|------------|
| `void motor_execute(void)` | 主状态机执行 | 调 `Key_Scan()`、`motor_start()`、`motor_stop()`、Vofa 输出 |
| `static void motor_open_speed(void)` | 按键调节目标占空比并斜坡逼近 | 调 `HAL_GetTick()` 做 1ms 节拍 |
| `static void motor_error_check(void)` | 检查过压/欠压/过温/堵转 | 调 `analog_check_xxx()` |
| `static void motor_stall_check(void)` | 堵转检测 | 基于 `bsp_hall_get_commutation_time()` 与 `HAL_GetTick()` |

###### `analog_calculate`

| 函数 | 功能 | HAL / 依赖 |
|------|------|------------|
| `void adc_value_calculate(void)` | 周期换算母线电压和温度 | 调 `bsp_adc_get_vbus_raw()`、`bsp_adc_get_temp_raw()` |
| `float analog_get_vbus_voltage(void)` | 返回 VBUS 电压 | 软件换算 |
| `float analog_get_temperature(void)` | 返回温度 | 软件换算 |
| `uint8_t analog_check_overvoltage(void)` | 过压判断 | 阈值比较 |
| `uint8_t analog_check_undervoltage(void)` | 欠压判断 | 阈值比较 |
| `uint8_t analog_check_overtemp(void)` | 过温判断 | 阈值比较 |
| `void Test_Phase5_Init(void)` | 初始化状态机测试环境 | 调 `motor_ctrl_init()`、按键初始化 |
| `void Test_Phase5_Loop(void)` | 跑按键控制和保护流程 | 调 `motor_execute()` |

##### Phase 5 HAL 替换说明

| 原裸机/标准库思路 | HAL / 当前工程替换 |
|------------------|---------------------|
| 电位器给定调速 | 现阶段改为 `Key_Scan()` 按键调速 |
| 保护判断直接读 ADC 寄存器 | 通过 `bsp_adc_*` 接口取值 |
| 主循环状态机直接散落在 `main` | 封装进 `motor_execute()` |
| 裸机故障停机 | 调 `motor_stop()` + `bsp_ctrl_sd_disable()` |

### 3.2 推荐测试过程

每个阶段建议统一采用以下流程：

1. `main.c` 只挂一个测试入口，例如 `Test_Phase1_Init()` / `Test_Phase1_Loop()`。
2. Vofa/串口只输出本阶段关心的数据，不要混合太多变量。
3. 功率测试和逻辑测试分离：先逻辑，再上驱动，再接电机。
4. 每一阶段都留“通过标准”，通过后再进入下一阶段。

推荐验证顺序：

| 阶段 | 主要验证对象 | 是否接电机 | 是否上功率母线 |
|------|--------------|------------|----------------|
| Phase0 | 串口/LED/按键/Vofa | 否 | 否 |
| Phase1 | PWM / 下桥 GPIO / Enable | 否 | 否 |
| Phase2 | ADC 采样 | 否 | 可选低压模拟 |
| Phase3 | Hall 状态 / 转速 | 可手转 | 否 |
| Phase4 | 六步换相 / 启停 | 是 | 是，但建议限流限压 |
| Phase5 | 状态机 / 保护 / PID | 是 | 是 |

---

## 4. 根据你现在已经添加的文件，建议的实施方案

你当前工程里，已经有这些用户层基础文件：

- `User/Bsp/Src/bsp_uart.c`
- `User/Bsp/Src/bsp_led.c`
- `User/Bsp/Src/bsp_key.c`
- `User/Bsp/Src/bsp_delay.c`
- `User/Vofa/Src/vofa_uart.c`
- `User/Vofa/Src/vofa_port.c`
- `User/Test/Src/test_phase0.c`

而 `Core/Src/main.c` 当前也明确只跑：

- `Test_Phase0_Init()`
- `Test_Phase0_Loop()`

这说明你的项目状态是：

- **基础调试层已经独立出来了**
- **还没有开始正式加入电机驱动层**
- **现在正适合按阶段加文件，而不是一次性复制整套 F407 代码**

### 4.1 最适合你的下一步文件方案

建议下一批只加 Phase1 文件：

| 路径 | 文件 |
|------|------|
| `User/Bsp/Inc` | `bsp_ctrl_sd.h` |
| `User/Bsp/Src` | `bsp_ctrl_sd.c` |
| `User/Bsp/Inc` | `bsp_pwm.h` |
| `User/Bsp/Src` | `bsp_pwm.c` |
| `User/Test/Inc` | `test_phase1.h` |
| `User/Test/Src` | `test_phase1.c` |

这一步完成后，不要急着加 Hall、ADC、电机控制文件。

### 4.2 在再添加文件前，是否还要做其他功能测试

要，建议先补 4 个“新增文件前检查项”。

#### 必测项 A：PWM 波形基础确认

虽然 TIM1 已经配置好了，但你现在还没有独立验证：

- 中心对齐是否真是 10kHz
- CH1/2/3 占空比改写是否生效
- MOE 与 `HAL_TIM_PWM_Start()` 的配合是否正确

#### 必测项 B：PWM_Enable 极性确认

你当前 `PC13` 初始已经是 `RESET`，这与原理图给出的“低电平时半桥芯片不工作”一致，属于安全默认态。

在驱动器使能文件落地前，建议先确认：

- `PC13=0` 时半桥是否确实禁止输出
- `PC13=1` 时半桥是否恢复工作
- 是否与原 F407 项目极性相反

#### 必测项 C：Break 输入逻辑确认

虽然 BKIN 已配置，但目前没有验证：

- 过流/故障信号是高有效还是低有效
- 当前极性设置是否只是“为了先避开误触发”

这项不确认，后面做保护逻辑时容易反着写。

#### 必测项 D：ADC2 触发链路先改好

虽然你 Phase1 还不写 ADC 驱动，但从工程组织角度，建议现在就把 `ADC2` 注入触发源改好。

因为后面一旦 `bsp_adc_motor.c` 开始写，就不需要再回头反复改 `.ioc`。

#### 必测项 E：控制板与驱动板分离时的上电策略

如果控制板和驱动板是分开的，Phase1 建议按下面顺序上电和验证，不要一开始就给 MOS 功率母线上电。

##### 1. Phase1 分板上电检查表

| 步骤 | 控制板电源 | 驱动板逻辑/驱动电源 | 功率母线 | 是否建议 |
|------|------------|----------------------|----------|----------|
| Step A | 上电 | 不上电 | 不上电 | 可做，适合先测 MCU 直出波形 |
| Step B | 上电 | 上电 | 不上电 | 推荐，适合验证驱动链路逻辑 |
| Step C | 上电 | 上电 | 上电 | Phase1 不建议，留到 Phase4 以后 |

结论：

- Phase1 **不需要**给带 MOS 的功率部分上正式工作母线电压。
- 但如果你要验证驱动板是否真正响应控制信号，**建议给驱动板本身的逻辑/栅极驱动电源上电**。
- 只有在后续开环带电机测试时，才建议接入功率母线，而且要限压限流。

##### 2. 每一步测什么、正常现象是什么

| 步骤 | 测试点 | 正常现象 |
|------|--------|----------|
| Step A | 控制板 `PC13` | 随 `bsp_ctrl_sd_enable/disable` 在高低电平间切换 |
| Step A | 控制板 `PA8/PA9/PA10` | 可测到 10kHz PWM 波形 |
| Step A | 控制板 `PB13/PB14/PB15` | 可测到普通 GPIO 高低翻转 |
| Step B | 驱动板输入端 `Enable/PWM/LIN` | 能跟随控制板信号变化 |
| Step B | 驱动芯片相关状态脚 | 使能后无异常锁死/无故障锁存 |
| Step B | MOS 栅极相关测试点（若板上预留） | 能看到驱动响应，但此时不要求带负载切换功率 |
| Step C | 电机相线/母线电流 | 这是后续 Phase4/Phase5 才进入的测试 |

##### 3. 当前最推荐的 Phase1 供电方式

- 控制板：上电
- 驱动板逻辑/驱动电源：上电
- 电机功率母线：不上电
- 电机：先不接或断开功率回路

##### 4. Phase1 的判断原则

- 如果你只是测 MCU 引脚波形，驱动板可以不上电。
- 如果你想确认驱动板输入到驱动芯片这一层是否正常，驱动板要上驱动电源。
- 如果你想确认 MOS 半桥实际切换，Phase1 不建议直接上功率母线。

##### 5. `Step0` ~ `Step5` 示波器测点表

建议优先准备 4 类测点：

- 控制板 `PC13`：驱动使能脚
- 控制板 `PA8/PA9/PA10`：三路上桥 PWM 输出
- 控制板 `PB13/PB14/PB15`：三路下桥逻辑输出
- 驱动板输入端：`Enable`、`HIN_U/V/W`、`LIN_U/V/W`（若板上有测试点）

| 步骤 | 推荐测点 | 预期结果 | 说明 |
|------|----------|----------|------|
| Step0 `SD disable, PWM off` | `PC13` | 低电平 | 半桥禁止态 |
| Step0 `SD disable, PWM off` | `PA8/PA9/PA10` | 无 PWM，保持低或静态 | 三路上桥关闭 |
| Step0 `SD disable, PWM off` | `PB13/PB14/PB15` | 全低电平 | 下桥全关 |
| Step1 `SD enable only` | `PC13` | 高电平 | 半桥允许工作 |
| Step1 `SD enable only` | `PA8/PA9/PA10` | 仍无 PWM | 这里只验证使能，不验证功率输出 |
| Step1 `SD enable only` | 驱动板 `Enable` 输入 | 与 `PC13` 同步为高 | 用来确认控制板到驱动板连线正确 |
| Step2 `CH1/2/3 50% PWM` | `PA8` | 10kHz，50% 占空比 | U 相 PWM |
| Step2 `CH1/2/3 50% PWM` | `PA9` | 10kHz，50% 占空比 | V 相 PWM |
| Step2 `CH1/2/3 50% PWM` | `PA10` | 10kHz，50% 占空比 | W 相 PWM |
| Step2 `CH1/2/3 50% PWM` | 驱动板 `HIN_U/V/W` | 与 `PA8/PA9/PA10` 同步 | 验证 PWM 已送到驱动板输入端 |
| Step3 `U phase PWM only` | `PA8` | 10kHz，50% 占空比 | 只有 U 相有 PWM |
| Step3 `U phase PWM only` | `PA9/PA10` | 无 PWM | 用于确认单相开通逻辑正确 |
| Step3 `U phase PWM only` | 驱动板 `HIN_U` | 有 PWM | 驱动板 U 相高边输入有效 |
| Step3 `U phase PWM only` | 驱动板 `HIN_V/HIN_W` | 无 PWM | 驱动板其余两相高边输入关闭 |
| Step4 `Lower UN/WN on` | `PB13` | 高电平 | U 相下桥逻辑打开 |
| Step4 `Lower UN/WN on` | `PB14` | 低电平 | V 相下桥逻辑关闭 |
| Step4 `Lower UN/WN on` | `PB15` | 高电平 | W 相下桥逻辑打开 |
| Step4 `Lower UN/WN on` | 驱动板 `LIN_U/LIN_W` | 高电平或对应有效电平 | 取决于驱动芯片输入极性 |
| Step5 `All outputs off` | `PC13` | 低电平 | 回到安全态 |
| Step5 `All outputs off` | `PA8/PA9/PA10` | 全部无 PWM | 上桥全关 |
| Step5 `All outputs off` | `PB13/PB14/PB15` | 全低电平 | 下桥全关 |

##### 5.1 `Step0` ~ `Step5` LED 对应规则

- 板载 LED 引脚对应关系：
  - `LED0 -> PE0`
  - `LED1 -> PE1`
- 当前 LED 为低电平点亮、高电平熄灭。

| Step | LED0 | LED1 | 对应引脚状态 | 说明 |
|------|------|------|--------------|------|
| `Step0` | 灭 | 灭 | `PE0=高`，`PE1=高` | 默认安全态，驱动禁止 |
| `Step1` | 亮 | 灭 | `PE0=低`，`PE1=高` | 仅使能驱动 |
| `Step2` | 灭 | 亮 | `PE0=高`，`PE1=低` | 三相 PWM 同时输出 |
| `Step3` | 亮 | 亮 | `PE0=低`，`PE1=低` | 仅 U 相 PWM 输出 |
| `Step4` | 闪烁 | 灭 | `PE0` 以 `200ms` 周期高低翻转，`PE1=高` | 下桥逻辑测试 |
| `Step5` | 灭 | 闪烁 | `PE0=高`，`PE1` 以 `200ms` 周期高低翻转 | 全关闭安全退出态 |

##### 5.2 `Step0` 和 `Step5` 的区别

- `Step0`：上电后默认测试起点，用于确认系统在“驱动禁止 + PWM 关闭 + 下桥全关”的初始状态下启动。
- `Step5`：测试中人工切入的安全退出态，用于确认所有输出都已经重新关闭。
- 两者都应表现为功率输出关闭，但 LED 提示不同，方便现场区分“初始态”和“测试结束/紧急回退态”。

##### 6. 示波器实测顺序建议

1. 先测 `PC13`，确认使能极性正确。
2. 再测 `PA8/PA9/PA10`，确认 PWM 频率和单相切换逻辑。
3. 再测 `PB13/PB14/PB15`，确认下桥 GPIO 状态。
4. 最后再到驱动板输入端测 `Enable/HIN/LIN` 是否正确跟随。

##### 7. Phase1 正常现象判断

- `Step0` 和 `Step5` 必须都是安全态。
- `Step2` 时三路 PWM 频率应一致、占空比一致。
- `Step3` 时只能有一相 PWM 输出，其余两相必须关闭。
- `Step4` 时不应再看到上桥 PWM，只验证下桥逻辑输出。
- 如果控制板波形正常、驱动板输入异常，优先检查排线、共地和驱动板电源。

### 4.3 当前最推荐的执行顺序

结合你现在的工程状态，我建议按这个顺序推进：

1. 先修改 `.ioc`：修正 `ADC2` 注入触发源，确认 Hall 上拉和 Break 极性。
2. 新增 `bsp_ctrl_sd`、`bsp_pwm`、`test_phase1`。
3. 完成纯波形测试，不接电机。
4. 再新增 `bsp_adc_motor`、`test_phase2`。
5. 再新增 `bsp_hall`、`test_phase3`。
6. 最后再引入 `motor_phase_tab`、`motor_ctrl`、`motor_execute`。

---

## 5. 简化版文件迁移清单

### 建议新增

| 阶段 | 文件 |
|------|------|
| Phase1 | `bsp_ctrl_sd.h/.c`, `bsp_pwm.h/.c`, `test_phase1.h/.c` |
| Phase2 | `bsp_adc_motor.h/.c`, `test_phase2.h/.c` |
| Phase3 | `bsp_hall.h/.c`, `test_phase3.h/.c` |
| Phase4 | `motor_phase_tab.h/.c`, `motor_ctrl.h/.c`, `test_phase4.h/.c` |
| Phase5 | `motor_execute.h/.c`, `analog_calculate.h/.c`, `pid.h/.c` |

### 建议不再单独保留

| 原工程文件类型 | 建议 |
|----------------|------|
| `*_cb.c` | 合并进对应模块 |
| `bsp_systick.*` | 不复刻 |
| `bsp_io.*` | 不复刻 |
| `bsp_dac.*` | 可不复刻 |
| `motor_sensor.c` 整体直搬 | 改为拆分迁移 |

---

## 6. 最后结论

- 你当前的 CubeMX 配置，**已经足够作为 BLDC Hall 复刻底座**。
- 现在最缺的不是“继续拷更多源文件”，而是 **先把配置链路和阶段测试链路理顺**。
- 原工程的回调文件在 HAL 工程里，建议 **合并到对应 BSP 模块中**，不要继续保留 `_cb.c` 结构。
- 从你当前已经添加的文件状态看，**最合理的下一步是进入 Phase1：PWM + 驱动使能测试**。
- 在再添加更多驱动文件前，建议先完成：`ADC2 注入触发修正`、`PC13 使能极性实测确认`、`Break 极性确认`、`PWM 波形基础确认`。

如果你愿意，下一步我可以直接继续帮你做两件事之一：

1. 直接生成 `Phase1` 需要的 `bsp_ctrl_sd`、`bsp_pwm`、`test_phase1` 文件骨架。
2. 先检查并修改你当前 `.ioc` / `tim.c` / `adc.c` 中与 Phase1、Phase2 相关的配置细节。
