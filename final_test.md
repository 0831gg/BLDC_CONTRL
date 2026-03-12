# BLDC Hall 移植 — 分阶段验证与实施清单

> 基于 `copyf407.md` 移植计划，按**严格验证顺序**排列。  
> 每个 Phase **必须全部通过**后才能进入下一个。  
> ✅ = 已完成  ⬜ = 待实施

---

## Phase 0 — 基础硬件自检 ✅ 已通过

| 测试项 | 结果 |
|--------|------|
| LED0 闪烁 (PE0, 500ms) | ✅ |
| KEY0/1/2 按键扫描 (PE12/13/14) | ✅ |
| USART1 阻塞发送 (PB6 TX, 115200) | ✅ |
| Vofa JustFloat 3 通道波形 (sin/cos/triangle) | ✅ |
| DWT 延时 (Delay_us / Delay_ms) | ✅ |

**已有文件**: `bsp_led`, `bsp_key`, `bsp_uart`, `bsp_delay`, `vofa_*`, `test_phase0`

---

## Phase 1 — PWM 输出 + 驱动器使能控制

### 1.1 需要新建的文件

| 文件 | 用途 |
|------|------|
| `User/Bsp/Inc/bsp_ctrl_sd.h` | 驱动器使能/失能接口 |
| `User/Bsp/Src/bsp_ctrl_sd.c` | PC13 (PWM_Enable) GPIO 控制 |
| `User/Bsp/Inc/bsp_pwm.h` | TIM1 PWM + 下桥 GPIO 接口 |
| `User/Bsp/Src/bsp_pwm.c` | TIM1 三相 PWM + PB13/14/15 下桥控制 + Break 回调 |

### 1.2 需要构建的函数

#### bsp_ctrl_sd (驱动器使能)

| 函数 | 功能 | HAL 调用 |
|------|------|----------|
| `void bsp_ctrl_sd_enable(void)` | 拉低 PC13 → 使能驱动器输出 | `HAL_GPIO_WritePin(GPIOC, GPIO_PIN_13, GPIO_PIN_RESET)` |
| `void bsp_ctrl_sd_disable(void)` | 拉高 PC13 → 禁止驱动器输出 | `HAL_GPIO_WritePin(GPIOC, GPIO_PIN_13, GPIO_PIN_SET)` |
| `void bsp_ctrl_sd_init(void)` | CubeMX 已配置，调用 `disable()` 确认安全态 | — |

> **注意**: 原 F407 使用 PF10 高电平使能。G474 的 PC13 默认 SET(高=禁止)，`enable()` 应拉低。请根据板子实际电路确认极性。

#### bsp_pwm (TIM1 三相 PWM + 下桥 GPIO)

| 函数 | 功能 | HAL 调用 |
|------|------|----------|
| `void bsp_pwm_init(void)` | 初始化 TIM1 PWM (CubeMX 已配置)，默认关闭全部输出，占空比=0 | `__HAL_TIM_SET_COMPARE(&htim1, TIM_CHANNEL_x, 0)` |
| `void bsp_pwm_duty_set(uint16_t duty)` | 同时设置 CH1/2/3 占空比 (0~4199) | `__HAL_TIM_SET_COMPARE(&htim1, TIM_CHANNEL_1/2/3, duty)` |
| `void bsp_pwm_all_open(void)` | 使能 TIM1 CH1/2/3 输出 + MOE | `HAL_TIM_PWM_Start(&htim1, ch)` × 3 + `__HAL_TIM_MOE_ENABLE(&htim1)` |
| `void bsp_pwm_all_close(void)` | 禁止 TIM1 CH1/2/3 输出 | `HAL_TIM_PWM_Stop(&htim1, ch)` × 3 |
| `void bsp_pwm_ctrl_single_phase(uint8_t phase)` | 只使能指定相上管 PWM，关闭其余 | `TIM1->CCER` 位操作 (CC1E/CC2E/CC3E) |
| `void bsp_pwm_lower_set(uint8_t un, uint8_t vn, uint8_t wn)` | 独立控制三路下桥 GPIO | `HAL_GPIO_WritePin(GPIOB, GPIO_PIN_13/14/15, level)` |
| `void bsp_pwm_lower_all_off(void)` | 关闭全部下桥 | PB13/14/15 全部 RESET |
| `void HAL_TIMEx_BreakCallback(TIM_HandleTypeDef *htim)` | Break 中断回调：关全部输出 + 失能驱动器 | `bsp_pwm_all_close()` + `bsp_ctrl_sd_disable()` |

> **下桥 GPIO 宏定义** (参照 F407 的 `MOS_xN_CTRL`):
> ```c
> #define MOS_UN_ON()   HAL_GPIO_WritePin(GPIOB, GPIO_PIN_13, GPIO_PIN_SET)
> #define MOS_UN_OFF()  HAL_GPIO_WritePin(GPIOB, GPIO_PIN_13, GPIO_PIN_RESET)
> #define MOS_VN_ON()   HAL_GPIO_WritePin(GPIOB, GPIO_PIN_14, GPIO_PIN_SET)
> #define MOS_VN_OFF()  HAL_GPIO_WritePin(GPIOB, GPIO_PIN_14, GPIO_PIN_RESET)
> #define MOS_WN_ON()   HAL_GPIO_WritePin(GPIOB, GPIO_PIN_15, GPIO_PIN_SET)
> #define MOS_WN_OFF()  HAL_GPIO_WritePin(GPIOB, GPIO_PIN_15, GPIO_PIN_RESET)
> ```

### 1.3 测试方案 (test_phase1)

| 序号 | 测试内容 | 验证方法 |
|------|----------|----------|
| 1 | `bsp_ctrl_sd_enable/disable` | 万用表测 PC13 电平 |
| 2 | `bsp_pwm_duty_set(2100)` → 50% 占空比 | 示波器看 PA8/PA9/PA10，确认 20kHz、50% |
| 3 | `bsp_pwm_ctrl_single_phase(U)` | 示波器确认只有 PA8 输出,PA9/PA10 无输出 |
| 4 | 下桥 GPIO 单独控制 | 万用表测 PB13/14/15 高低电平 |
| 5 | `bsp_pwm_all_close()` 后全部关断 | 示波器确认无输出 |
| 6 | Break 保护测试 (可选) | 短接 PB12 触发极性 → 观察 PWM 是否立即关闭 |

### 1.4 通过标准

- [ ] PA8/PA9/PA10 输出 10kHz PWM，占空比可调
- [ ] PB13/PB14/PB15 可独立控制高低电平
- [ ] PC13 使能/失能正确
- [ ] `all_close()` 后全部输出为低
- [ ] **不接电机、不上功率电**的情况下完成全部测试

---

## Phase 2 — ADC 采样验证

### 2.0 CubeMX 配置修改 (Phase 2 前必须完成)

| 修改项 | 当前值 | 改为 |
|--------|--------|------|
| ADC2 注入触发源 | `SOFTWARE_START` | `Timer 1 Trigger Out event` + Rising Edge |

> CubeMX 操作: ADC2 → Injected Conversion → External Trigger Source → **Timer 1 Trigger Out event** → Edge = Rising

### 2.1 需要新建的文件

| 文件 | 用途 |
|------|------|
| `User/Bsp/Inc/bsp_adc_motor.h` | ADC 数据读取接口 |
| `User/Bsp/Src/bsp_adc_motor.c` | ADC1 DMA 规则 + ADC2 注入采样 + 物理量换算 |

### 2.2 需要构建的函数

| 函数 | 功能 | HAL 调用 |
|------|------|----------|
| `void bsp_adc_init(void)` | 启动 ADC1 DMA + ADC2 校准 + 启动注入 | `HAL_ADC_Start_DMA(&hadc1, ...)` + `HAL_ADCEx_Calibration_Start(&hadc2, ...)` |
| `uint16_t bsp_adc_get_vbus_raw(void)` | 返回母线电压 ADC 原始值 | 读 DMA 缓冲区 `adc1_dma_buf[0]` |
| `uint16_t bsp_adc_get_temp_raw(void)` | 返回温度 ADC 原始值 | 读 DMA 缓冲区 `adc1_dma_buf[1]` |
| `float bsp_adc_get_vbus(void)` | 返回母线电压 (V) | `raw / 4095.0 × 3.3 × 分压比` |
| `float bsp_adc_get_temp(void)` | 返回温度 (°C) | NTC Steinhart-Hart 公式 |
| `void bsp_adc_get_phase_current(int16_t *iu, int16_t *iv, int16_t *iw)` | 返回三相电流 ADC 注入值 | `HAL_ADCEx_InjectedGetValue(&hadc2, ADC_INJECTED_RANK_1/2/3)` |
| `void HAL_ADCEx_InjectedConvCpltCallback(ADC_HandleTypeDef *hadc)` | ADC2 注入完成回调：缓存三相电流 | 读取注入结果到全局变量 |

> **ADC 通道映射对照 (F407 → G474)**:
>
> | 信号 | F407 | G474 | 方式 |
> |------|------|------|------|
> | VBUS | ADC1 IN9 (PB1) | **ADC1 CH4 (PA3)** | 规则+DMA |
> | TEMP | ADC1 IN0 (PA0) | **ADC1 CH6 (PC0)** | 规则+DMA |
> | SPEED 电位器 | ADC1 IN10 (PC0) | **无** (改用按键调速) | — |
> | 电流 U | ADC1 IN8 (PB0) | **ADC2 CH7 (PC1)** | 注入 TIM1 触发 |
> | 电流 V | ADC1 IN6 (PA6) | **ADC2 CH8 (PC2)** | 注入 TIM1 触发 |
> | 电流 W | ADC1 IN3 (PA3) | **ADC2 CH9 (PC3)** | 注入 TIM1 触发 |
> | BEMF U/V/W | ADC3 注入 | **无** (有感模式不需要) | — |

### 2.3 测试方案 (test_phase2)

| 序号 | 测试内容 | 验证方法 |
|------|----------|----------|
| 1 | VBUS 原始值打印 | Vofa 实时曲线 + 万用表对照 |
| 2 | TEMP 原始值打印 | Vofa 实时曲线 + 手触温度变化 |
| 3 | VBUS/TEMP 换算为物理量 (V/°C) | 串口打印，对照万用表 |
| 4 | 启动 TIM1 后 ADC2 注入自动触发 | 打印三相电流 ADC 值，确认非 0 且稳定 |
| 5 | Vofa 4 通道曲线 (VBUS, TEMP, Iu, Iv) | 长时间观察无乱跳 |

### 2.4 通过标准

- [ ] VBUS 读数与万用表偏差 < 0.5V
- [ ] TEMP 数据随温度变化
- [ ] 三相电流静态值稳定 (零飘在合理范围)
- [ ] TIM1 触发 ADC2 注入结果每 PWM 周期更新一次

---

## Phase 3 — Hall 位置与速度

### 3.1 需要新建的文件

| 文件 | 用途 |
|------|------|
| `User/Bsp/Inc/bsp_hall.h` | Hall 传感器读取 + 速度计算接口 |
| `User/Bsp/Src/bsp_hall.c` | EXTI 回调 + TIM5 时间戳测速 |

### 3.2 需要构建的函数

| 函数 | 功能 | HAL 调用 |
|------|------|----------|
| `void bsp_hall_init(void)` | 启动 TIM5 自由运行计时器 | `HAL_TIM_Base_Start(&htim5)` |
| `uint8_t bsp_hall_get_state(void)` | 读 PA0/PA1/PA2 组合为 3bit Hall 值 (1~6) | `HAL_GPIO_ReadPin(GPIOA, HALL_U/V/W_Pin)` |
| `uint32_t bsp_hall_get_speed(void)` | 返回当前转速 RPM | 内部计算 |
| `uint32_t bsp_hall_get_commutation_time(void)` | 返回最近换相时刻 (TIM5 CNT) | 内部读取 |
| `void bsp_hall_irq_enable(void)` | 使能 EXTI0/1/2 中断 | `HAL_NVIC_EnableIRQ(EXTI0_IRQn/1/2)` |
| `void bsp_hall_irq_disable(void)` | 禁止 EXTI0/1/2 中断 | `HAL_NVIC_DisableIRQ(...)` |
| `void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin)` | Hall 中断回调：记录时间戳 + 计算转速 + 调用换相函数 (Phase 4 才挂载) | `__HAL_TIM_GET_COUNTER(&htim5)` |

> **Hall 引脚映射 (F407 → G474)**:
>
> | 信号 | F407 | G474 |
> |------|------|------|
> | HALL_U | PH10 (EXTI15_10 共享) | **PA0 (EXTI0 独立)** |
> | HALL_V | PH11 (EXTI15_10 共享) | **PA1 (EXTI1 独立)** |
> | HALL_W | PH12 (EXTI15_10 共享) | **PA2 (EXTI2 独立)** |

> **速度计算 (替换 F407 的 TIM4 10μs 中断累加方案)**:
>
> F407 原方案: TIM4 每 10μs 中断 → `timer_us_counter++` → Hall 跳变时读差值  
> G474 新方案: TIM5 32 位自由运行 (100kHz, 10μs/tick) → Hall 跳变时直读 `TIM5->CNT` 求差  
> ```
> RPM = 60,000,000 / (delta_cnt × 10 × 2 × MOTOR_PAIR_OF_POLES)
> ```
> 其中 `delta_cnt` = 两次 U 相同方向跳变的 TIM5 计数差 (一个电周期)

### 3.3 测试方案 (test_phase3)

| 序号 | 测试内容 | 验证方法 |
|------|----------|----------|
| 1 | 静态读 Hall 三位值 | 手转电机，串口打印 UVW 位组合 |
| 2 | 验证 6 种合法 Hall 状态 | 缓慢转动，确认 1→5→4→6→2→3 顺序 (或反序) |
| 3 | EXTI 中断计数 | 打印中断触发次数,手转时计数增加 |
| 4 | TIM5 时间戳差值 | 打印相邻跳变间隔 (应随转速变化) |
| 5 | RPM 计算 | 匀速手转 ~60 RPM，串口/Vofa 验证 |
| 6 | Vofa 2 通道 (Hall_state, RPM) | 曲线平滑无乱跳 |

### 3.4 通过标准

- [ ] Hall 六种合法态循环出现，无非法码 (0 或 7)
- [ ] 正、反方向 Hall 顺序相反
- [ ] RPM 随手转快慢明显变化
- [ ] 长时间慢摇无异常跳变

---

## Phase 4 — 六步换相开环运行

### 4.1 需要新建的文件

| 文件 | 用途 |
|------|------|
| `User/Motor/Inc/motor_phase_tab.h` | 六步换相函数声明 |
| `User/Motor/Src/motor_phase_tab.c` | 六步换相表实现 |
| `User/Motor/Inc/motor_ctrl.h` | 电机控制参数结构体 + 启停接口 |
| `User/Motor/Src/motor_ctrl.c` | 电机启停流程 |

### 4.2 需要构建的函数

#### motor_phase_tab (六步换相表)

6 个换相函数，每个完成: ① 设置下桥 GPIO ② 使能对应上管 PWM ③ 设置占空比

| 函数 | 导通路径 | 上管 PWM | 下桥 ON |
|------|----------|----------|---------|
| `void mos_up_vn(uint16_t duty)` | U+ V- | CH1 (PA8) | PB14 |
| `void mos_up_wn(uint16_t duty)` | U+ W- | CH1 (PA8) | PB15 |
| `void mos_vp_un(uint16_t duty)` | V+ U- | CH2 (PA9) | PB13 |
| `void mos_vp_wn(uint16_t duty)` | V+ W- | CH2 (PA9) | PB15 |
| `void mos_wp_un(uint16_t duty)` | W+ U- | CH3 (PA10) | PB13 |
| `void mos_wp_vn(uint16_t duty)` | W+ V- | CH3 (PA10) | PB14 |

> 内部实现模板:
> ```c
> void mos_up_vn(uint16_t duty) {
>     MOS_UN_OFF(); MOS_VN_ON(); MOS_WN_OFF();       // 下桥: 只 V 导通
>     bsp_pwm_ctrl_single_phase(U_PHASE);             // 上管: 只 U 相 PWM
>     bsp_pwm_duty_set(duty);                         // 设置占空比
> }
> ```

#### motor_ctrl (电机控制核心)

| 函数 | 功能 |
|------|------|
| `void motor_ctrl_init(void)` | 清零控制参数,初始化状态为 STOP |
| `int motor_start(uint16_t duty, uint8_t direction)` | 启动流程: ①复位锁存 ②使能半桥 ③自举充电(下桥全开 200ms) ④读 Hall ⑤首次换相 |
| `void motor_stop(void)` | 停止流程: ①关 Hall 中断 ②PWM=0 ③关上管+下桥 ④失能半桥 |
| `void motor_sensor_mode_phase(void)` | 被 Hall 中断调用: 读 Hall → 查表换相 (根据方向和 Hall 状态选择对应的 `mos_xp_yn()`) |

> **关键结构体** (对应 F407 的 `motor_ctrl_param_t`):
> ```c
> typedef struct {
>     uint16_t pwm_duty;           // 当前占空比
>     uint8_t  motor_direction;    // 0=CW, 1=CCW
>     uint8_t  motor_sta;          // STOP/RUN/FAULT
>     uint32_t calculate_speed;    // 计算转速 RPM
>     uint32_t motor_phase_time;   // 最近换相时刻
>     uint8_t  error_type;         // 故障类型
> } motor_ctrl_param_t;
> ```

### 4.3 Hall → 换相映射表 (来自 F407 motor_sensor.c)

**CW (顺时针) 方向:**

| Hall值 (UVW) | 3bit | 换相动作 |
|---|---|---|
| 1 0 1 | 5 | `mos_up_vn(duty)` — U+ V- |
| 0 0 1 | 1 | `mos_up_wn(duty)` — U+ W- |
| 0 1 1 | 3 | `mos_vp_wn(duty)` — V+ W- |
| 0 1 0 | 2 | `mos_vp_un(duty)` — V+ U- |
| 1 1 0 | 6 | `mos_wp_un(duty)` — W+ U- |
| 1 0 0 | 4 | `mos_wp_vn(duty)` — W+ V- |

**CCW (逆时针) 方向:** 换相表反转（同 Hall 值执行相反的导通对）

> ⚠️ 此映射表从 F407 源码提取，需在实际硬件上验证顺序是否正确。如果电机转动方向错误或抖动，需交换任意两相。

### 4.4 测试方案 (test_phase4)

| 序号 | 测试内容 | 验证方法 |
|------|----------|----------|
| 1 | 自举充电 (下桥全开 200ms) | 示波器看 PB13/14/15 同时为高 200ms |
| 2 | 固定 5% 占空比 (duty=210) 启动 | 电机低速转动 |
| 3 | 固定 10% 占空比 (duty=420) | 电机中速平稳运转 |
| 4 | 正转 → 停止 → 反转 | 方向正确切换 |
| 5 | Vofa 监控: RPM + duty + Hall_state | 波形稳定 |
| 6 | 电流监控: Iu/Iv/Iw | 无过流尖峰 |

### 4.5 通过标准

- [ ] 电机可低速 (~500 RPM) 稳定开环运行
- [ ] 换相平顺，无明显抖动/卡顿
- [ ] 正反转可切换
- [ ] 电流尖峰在允许范围内
- [ ] **首次上电务必限制占空比 ≤10%**

---

## Phase 5 — 按键控制 + 执行状态机

### 5.1 需要新建的文件

| 文件 | 用途 |
|------|------|
| `User/Motor/Inc/motor_execute.h` | 执行状态机接口 |
| `User/Motor/Src/motor_execute.c` | 按键响应 + 速度斜坡 + 保护检测 |
| `User/App/Inc/analog_calculate.h` | ADC 物理量换算 + 保护阈值 |
| `User/App/Src/analog_calculate.c` | 母线电压/温度换算 + 过压欠压过温检测 |

### 5.2 需要构建的函数

#### motor_execute (执行状态机)

| 函数 | 功能 |
|------|------|
| `void motor_execute(void)` | **主循环调用**: 按键处理 + 状态机(IDLE→START→RUN→STOP) + 保护检测 + Vofa 输出 |
| `static void motor_open_speed(void)` | 开环速度控制: KEY2 调整目标 PWM，每 1ms 斜坡逼近 |
| `static void motor_error_check(void)` | 综合保护: 过压/欠压/过温/堵转 → 停机 |
| `static void motor_stall_check(void)` | 堵转检测: 500ms 内无换相 → 停机 |

> **按键映射** (替代 F407 的电位器调速):
>
> | 按键 | 短按功能 | 长按功能 |
> |------|----------|----------|
> | KEY0 (PE12) | 启动/停止 | — |
> | KEY1 (PE13) | 切换方向 (CW↔CCW) | — |
> | KEY2 (PE14) | 加速 (+5% duty) | 减速 (-5% duty) |

> **状态机** (对应 F407 的 `motor_execute_state_machine_e`):
> ```
> IDLE → (KEY0 按下) → START → (启动成功) → RUN → (KEY0 按下/故障) → STOP → IDLE
>                                              ↑                        |
>                                              └── (故障恢复/KEY0 重按) ─┘
> ```

#### analog_calculate (ADC 物理量换算)

| 函数 | 功能 | 公式/阈值 |
|------|------|-----------|
| `void adc_value_calculate(void)` | 每 10ms 调用,换算 VBUS/TEMP | 见下方 |
| `float analog_get_vbus_voltage(void)` | 返回母线电压 (V) | `ADC / 4095 × 3.3 × 分压比` |
| `float analog_get_temperature(void)` | 返回温度 (°C) | NTC Steinhart-Hart |
| `uint8_t analog_check_overvoltage(void)` | 过压检测 | VBUS > 28V |
| `uint8_t analog_check_undervoltage(void)` | 欠压检测 | VBUS < 20V |
| `uint8_t analog_check_overtemp(void)` | 过温检测 | TEMP > 80°C |

> **温度换算 (NTC, 来自 F407 analog_calculate.c)**:
> ```c
> Vr = adc_raw / 4095.0f * 3.3f;
> Rt = (3.3f - Vr) * 4700.0f / Vr;   // 上拉 4.7kΩ
> temp = 1.0f / (1.0f/298.15f + logf(Rt/10000.0f)/3455.0f) - 273.15f;
> ```
> ⚠️ 电阻值需根据实际电路确认 (F407 原始: R_pullup=4.7kΩ, NTC R25=10kΩ, B=3455)

### 5.3 测试方案 (test_phase5)

| 序号 | 测试内容 | 验证方法 |
|------|----------|----------|
| 1 | KEY0 启动 → 电机转 → KEY0 停止 | 反复操作 |
| 2 | KEY1 切换方向 (先停→换→再启) | 方向正确 |
| 3 | KEY2 短按加速 / 长按减速 | Vofa 看 RPM 曲线随操作变化 |
| 4 | 速度斜坡 (不是瞬间跳变) | Vofa 看 duty 缓慢变化 |
| 5 | Vofa 多通道: RPM, duty, VBUS, TEMP | 长时间稳定 |
| 6 | 堵转测试 (手握电机轴) | 500ms 后自动停机 |

### 5.4 通过标准

- [ ] 按键控制启停/方向/速度可靠
- [ ] 速度斜坡平滑
- [ ] 堵转保护生效 (500ms 内无换相 → 停机)
- [ ] 长时间运行 (>5min) 无异常

---

## Phase 6 — PID 闭环 + 完整保护

### 6.1 需要新建的文件

| 文件 | 用途 |
|------|------|
| `User/PID/Inc/pid.h` | PID 控制器结构体 + 算法接口 |
| `User/PID/Src/pid.c` | 增量式 PID 实现 |

### 6.2 需要构建的函数

#### pid (增量式 PID)

| 函数 | 功能 |
|------|------|
| `void incremental_pid(pid_ctrl_t *pid)` | 增量式 PID: `Δuk = Kp(e-e1) + Ki·e + Kd(e-2e1+e2)`，输出限幅 |

> **PID 结构体** (来自 F407):
> ```c
> typedef struct {
>     float kp, ki, kd;
>     float target_value, actual_value;
>     float error, last_error, prev_error;
>     float uk_value;              // 累积输出 (即 PWM duty)
>     float uk_max_value;          // 输出上限 = MAX_PWM_DUTY (4116)
>     float uk_min_value;          // 输出下限 = MIN_PWM_DUTY (420)
> } pid_ctrl_t;
> ```

> **PID 参数** (F407 初始值):
> - Kp = 0.3, Ki = 0.03, Kd = 0
> - 采样周期 = 一个电周期 (360° 电角度时间)
> - 目标: 按键设定的目标转速 (RPM)
> - 反馈: Hall 测量的 calculate_speed (RPM)
> - 输出: pwm_duty

#### motor_execute 修改

| 修改 | 说明 |
|------|------|
| 新增 `motor_pid_init(void)` | 初始化 PID 参数: kp=0.3, ki=0.03, kd=0, min/max |
| 新增 `motor_pid_speed(void)` | 闭环调速: 每个电周期调用一次 PID，输出→duty |
| `motor_execute()` 中切换 | 增加开环/闭环模式切换 (可用 KEY2 长按切换) |

### 6.3 保护逻辑完善

| 保护项 | 阈值 | 动作 | 恢复条件 |
|--------|------|------|----------|
| 过压 | VBUS > 28V, 连续 5 次 | 停机 | VBUS < 27V 持续 5 次 |
| 欠压 | VBUS < 20V, 连续 5 次 | 停机 | VBUS > 21V 持续 5 次 |
| 过温 | TEMP > 80°C, 连续 5 次 | 停机 | TEMP < 70°C 持续 5 次 |
| 堵转 | 500ms 无换相 | 停机 | 需 KEY0 手动重启 |
| Break | PB12 触发 | 硬件停机 | 需软件恢复 `__HAL_TIM_MOE_ENABLE` |
| 过流 | Break 中断 | 关全桥+失能 | 需 KEY0 重启 |

### 6.4 测试方案 (test_phase6)

| 序号 | 测试内容 | 验证方法 |
|------|----------|----------|
| 1 | PID 阶跃响应 (1000→2000 RPM) | Vofa 看 RPM 超调/稳态精度 |
| 2 | 轻载→加载→恢复 | RPM 能恢复目标值 |
| 3 | 调 Kp/Ki 观察响应变化 | Vofa 对比波形 |
| 4 | 过压保护 (升高供电电压) | 自动停机 + 串口报错类型 |
| 5 | 过温保护 (用热风枪) | 自动停机 |
| 6 | 故障后 KEY0 恢复 | 可重新启动 |
| 7 | 长时间闭环运行 (>30min) | 无异常/无失控 |

### 6.5 通过标准

- [ ] PID 跟踪目标转速 (误差 < 5%)
- [ ] 超调量 < 20%
- [ ] 各保护功能正常触发和恢复
- [ ] 长时间运行稳定

---

## 附录 A — 文件创建总览

按 Phase 顺序的完整新建文件列表：

| Phase | 文件 | Keil 工程组 |
|-------|------|-------------|
| 1 | `bsp_ctrl_sd.h/.c` | User/Bsp |
| 1 | `bsp_pwm.h/.c` | User/Bsp |
| 2 | `bsp_adc_motor.h/.c` | User/Bsp |
| 3 | `bsp_hall.h/.c` | User/Bsp |
| 4 | `motor_phase_tab.h/.c` | User/Motor |
| 4 | `motor_ctrl.h/.c` | User/Motor |
| 5 | `motor_execute.h/.c` | User/Motor |
| 5 | `analog_calculate.h/.c` | User/App |
| 6 | `pid.h/.c` | User/PID |

> 每个 Phase 新增的 .c 文件需添加到 Keil `.uvprojx` 对应 Group，.h 目录需添加到 IncludePath

---

## 附录 B — 不再创建的文件 (F407 → G474 删除项)

| F407 文件 | 原因 |
|-----------|------|
| `bsp_hall_exti_cb.c` | 合并进 `bsp_hall.c` 的 `HAL_GPIO_EXTI_Callback` |
| `bsp_hall_speed_cb.c` | TIM5 自由运行替代 TIM4 10μs 中断，不需要 |
| `bsp_pwm_cb.c` | 合并进 `bsp_pwm.c` 的 `HAL_TIMEx_BreakCallback` |
| `bsp_adc_cb.c` | 合并进 `bsp_adc_motor.c` 的 `HAL_ADCEx_InjectedConvCpltCallback` |
| `bsp_uart_cb.c` | 合并进 `bsp_uart.c` (当前无 RX 需求) |
| `bsp_systick.c` | HAL 的 `HAL_GetTick()` 替代 |
| `bsp_io.c` | CubeMX 的 `MX_GPIO_Init()` 替代 |
| `bsp_dac.c` | Vofa 串口替代 DAC 调试输出 |
| `motor_sensor.c` | 拆分合并：Hall 读取→`bsp_hall.c`，换相逻辑→`motor_ctrl.c` |

---

## 附录 C — HAL 关键 API 快速参考

| 操作 | HAL 调用 |
|------|----------|
| 下桥开关 | `HAL_GPIO_WritePin(GPIOB, GPIO_PIN_13, SET/RESET)` |
| 读 Hall | `HAL_GPIO_ReadPin(GPIOA, GPIO_PIN_0/1/2)` |
| 设占空比 | `__HAL_TIM_SET_COMPARE(&htim1, TIM_CHANNEL_x, duty)` |
| PWM 单相使能 | `TIM1->CCER \|= TIM_CCER_CC1E` |
| PWM 单相禁止 | `TIM1->CCER &= ~TIM_CCER_CC1E` |
| PWM 全开 | `HAL_TIM_PWM_Start(&htim1, ch)` × 3 |
| PWM 全关 | `HAL_TIM_PWM_Stop(&htim1, ch)` × 3 |
| MOE 使能 | `__HAL_TIM_MOE_ENABLE(&htim1)` |
| 读注入 ADC | `HAL_ADCEx_InjectedGetValue(&hadc2, rank)` |
| 读速度计数 | `__HAL_TIM_GET_COUNTER(&htim5)` |
| 启动 ADC DMA | `HAL_ADC_Start_DMA(&hadc1, buf, len)` |
| ADC 校准 | `HAL_ADCEx_Calibration_Start(&hadc2, ADC_SINGLE_ENDED)` |
| SysTick | `HAL_GetTick()` |
