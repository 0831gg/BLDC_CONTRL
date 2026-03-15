# 实验9-1 与 实验9-3 工程差异对比

## 1. 对比对象

- `9-1`：`C:\Users\王子龙\Desktop\正点原子\正点原子电机控制源码\实验9 BLDC专题实验\实验9-1 BLDC基础驱动`
- `9-3`：`C:\Users\王子龙\Desktop\正点原子\正点原子电机控制源码\实验9 BLDC专题实验\实验9-3 BLDC电源电压-温度-三相电流采集`

## 2. 总体结论

`9-3` 不是单纯在 `9-1` 上加显示信息，而是把工程重点从“BLDC 基础驱动 + PWM/VOFA 调试”切换为“BLDC 驱动 + ADC 采样 + 电压/温度/三相电流测量”。

可以把两者的定位理解为：

- `9-1`：验证 BLDC 六步换相、正反转、占空比调节、PWM 输出和 VOFA 上位机调试。
- `9-3`：在保留基础驱动的前提下，引入 ADC + DMA + 定时采样，采集母线电压、驱动板温度、三相电流和母线电流。

## 3. 差异统计

- `9-1` 文件总数：`344`
- `9-3` 文件总数：`314`
- 仅 `9-1` 存在：`38` 个文件
- 仅 `9-3` 存在：`8` 个文件
- 同名但内容不同：`52` 个文件

说明：

- 上述 `52` 个同名差异文件里，很多属于 `Output` 目录下的编译产物，功能分析时应忽略。
- 真正影响功能的核心差异主要集中在：
  - `User/main.c`
  - `Drivers/BSP/BLDC/bldc.c`
  - `Drivers/BSP/BLDC/bldc.h`
  - `Drivers/BSP/TIMER/bldc_tim.c`
  - `Drivers/BSP/TIMER/bldc_tim.h`
  - `Drivers/BSP/ADC/adc.c`
  - `Drivers/BSP/ADC/adc.h`
  - `Projects/MDK-ARM/atk_g474.uvprojx`

## 4. 目录层面的主要不同

### 4.1 仅 `9-1` 存在的关键内容

#### BLDC 额外封装层

- `Drivers/BSP/BLDC/bldc_hal_interface.c`
- `Drivers/BSP/BLDC/bldc_hal_interface.h`
- `Drivers/BSP/BLDC/bldc_motor.c`
- `Drivers/BSP/BLDC/bldc_motor.h`

这组文件的作用是把 BLDC 控制逻辑与底层 HAL/引脚操作分离，形成一层额外的“硬件抽象接口”和“电机逻辑封装”。从代码结构看，它更偏向可移植/可重构版本，而 `9-3` 没有保留这层拆分，功能直接并入现有 BSP 驱动。

#### VOFA 调试模块

- `User/Vofa/Inc/vofa_motor.h`
- `User/Vofa/Inc/vofa_port.h`
- `User/Vofa/Inc/vofa_port_config.h`
- `User/Vofa/Inc/vofa_uart.h`
- `User/Vofa/Src/vofa_motor.c`
- `User/Vofa/Src/vofa_port.c`
- `User/Vofa/Src/vofa_uart.c`

这说明 `9-1` 带有串口上位机调试/波形观察能力，而 `9-3` 已经移除这部分。

#### 其他

- `.gitignore`
- `Projects/MDK-ARM/Makefile`
- `.github`、`.vscode`、若干 `uv4_*.log`

这些更偏向工程辅助文件，不属于核心业务逻辑。

### 4.2 仅 `9-3` 存在的关键内容

- `Drivers/BSP/ADC/adc.c`
- `Drivers/BSP/ADC/adc.h`

这是 `9-3` 最核心的新增模块，说明电压/温度/电流采集是通过独立 ADC 驱动模块实现的。

另外还有：

- `Output/adc.*`
- `Output/stm32g4xx_hal_adc*.o`

这些只是新增 ADC 模块后的编译产物。

## 5. 核心源码差异

### 5.1 `User/main.c`

#### `9-1` 的主流程特点

关键位置：

- `User/main.c:18` 定义 `bldc_pwm_test()`
- `User/main.c:64` 调用 `vofa_motor_init()`
- `User/main.c:70` LCD 提示 `UART 't' for PWM test`
- `User/main.c:108` 周期调用 `vofa_motor_send_state()`
- `User/main.c:112` / `137` / `162` 处理 `KEY0/KEY1/KEY2`

功能特征：

- 支持 UART 输入 `'t'` 进入 PWM 测试模式。
- 支持 VOFA 上位机状态发送。
- 主要关注电机正转、反转、停机和 PWM 占空比变化。
- LCD 显示更偏向“控制”和“调试提示”。

#### `9-3` 的主流程特点

关键位置：

- `User/main.c:51` 调用 `btim_timx_int_init(1000 - 1, 170 - 1)`
- `User/main.c:53` 调用 `adc_dma_init()`
- `User/main.c:69` 显示电源电压 `g_adc_val[0] * ADC2VBUS`
- `User/main.c:71` 显示温度 `get_temp(g_adc_val[1])`
- `User/main.c:80` 到 `83` 对三相电流和母线电流做一阶低通滤波
- `User/main.c:103` 到 `108` 串口打印电压、温度、U/V/W 相电流、母线电流
- `User/main.c:113` / `131` / `149` 处理 `KEY0/KEY1/KEY2`

功能特征：

- 新增基本定时器中断，用于定时采样和偏置更新。
- 新增 ADC DMA 采样。
- 新增 LCD 和串口上的电压、温度、电流数据显示。
- 删除了 PWM 测试模式。
- 删除了 VOFA 调试输出。

#### 结论

`9-1` 的 `main.c` 更像“驱动验证与调试入口”，`9-3` 的 `main.c` 更像“带采样监测的运行演示入口”。

### 5.2 `Drivers/BSP/BLDC/bldc.h`

`9-3` 相比 `9-1` 新增了以下与采样相关的宏和声明：

- `ADC2CURT`
- `ADC2VBUS`
- `NUM_CLEAR`
- `NUM_MAX_LIMIT`
- `NUM_MIN_LIMIT`
- `FirstOrderRC_LPF(...)`
- `float get_temp(uint16_t para);`

含义很明确：

- `ADC2VBUS`：把 ADC 原始值转换为母线电压。
- `ADC2CURT`：把 ADC 原始值转换为电流值。
- `FirstOrderRC_LPF`：对电流显示做一阶低通滤波。
- `get_temp()`：把温度采样值换算成摄氏温度。

这部分是 `9-3` 采样链路的基础公式层。

### 5.3 `Drivers/BSP/BLDC/bldc.c`

关键位置：

- `Drivers/BSP/BLDC/bldc.c:278` 定义热敏电阻参数 `Rp`
- `Drivers/BSP/BLDC/bldc.c:291` 定义 `get_temp(uint16_t para)`
- `Drivers/BSP/BLDC/bldc.c:298` 使用 `log()` 参与温度换算

差异说明：

- `9-3` 在原 BLDC 驱动文件尾部直接加入了温度换算逻辑。
- 这个温度计算基于热敏电阻 B 值公式，把 ADC 采样值先还原为电阻值，再计算温度。
- `9-1` 中没有温度采集和换算功能。

### 5.4 `Drivers/BSP/TIMER/bldc_tim.h`

`9-3` 相比 `9-1` 增加了基本定时器相关定义：

- `BTIM_TIMX_INT`
- `BTIM_TIMX_INT_IRQn`
- `BTIM_TIMX_INT_IRQHandler`
- `BTIM_TIMX_INT_CLK_ENABLE()`
- `void btim_timx_int_init(uint16_t arr, uint16_t psc);`

说明 `9-3` 在原来的 TIM1 PWM/换相定时器之外，又加入了一个基础定时器用于周期性采样处理。

### 5.5 `Drivers/BSP/TIMER/bldc_tim.c`

这是 `9-3` 相比 `9-1` 差异最大、价值最高的文件之一。

关键位置：

- `Drivers/BSP/TIMER/bldc_tim.c:161` 新增 `btim_timx_int_init()`
- `Drivers/BSP/TIMER/bldc_tim.c:211` 定义 `ADC_AMP_OFFSET_TIMES`
- `Drivers/BSP/TIMER/bldc_tim.c:224` 扩展 `HAL_TIM_PeriodElapsedCallback()`
- `Drivers/BSP/TIMER/bldc_tim.c:255` 用运行时电流采样值减去停机偏置
- `Drivers/BSP/TIMER/bldc_tim.c:264` 到 `284` 计算母线电流
- `Drivers/BSP/TIMER/bldc_tim.c:289` 到 `310` 在停机状态下统计三相电流偏置

`9-3` 增加的逻辑主要包括：

- 新增 TIM6 基本定时器中断。
- 停机时对 U/V/W 三相电流 ADC 值进行多次采样平均，得到零偏。
- 运行时用“当前采样值 - 停机偏置值”计算有效相电流。
- 对负值进行抑制，减少反电动势或噪声带来的负电流显示。
- 按当前换相状态组合相电流，推导母线电流 `adc_amp_bus`。

而 `9-1` 的 `bldc_tim.c` 主要职责只有：

- TIM1 PWM 输出初始化
- 霍尔状态读取后的六步换相
- 电机运行/停机切换

也就是说，`9-3` 把“电机驱动中断”扩展成了“驱动 + 采样计算”的复合中断处理中心。

### 5.6 `Drivers/BSP/ADC/adc.h` 与 `adc.c`

这两个文件仅存在于 `9-3`。

关键位置：

- `Drivers/BSP/ADC/adc.c:121` `adc_dma_init()`
- `Drivers/BSP/ADC/adc.c:170` `HAL_ADC_ConvCpltCallback()`
- `Drivers/BSP/ADC/adc.c:220` `calc_adc_val()`

实现要点：

- 使用 `ADC1` 做 5 路规则组采样：
  - 电源电压
  - 温度
  - U 相电流
  - V 相电流
  - W 相电流
- 使用 DMA 循环搬运 ADC 数据。
- 在 DMA 完成回调里做均值滤波，得到 `g_adc_val[]`。
- 后续 `main.c` 和 `bldc_tim.c` 都直接依赖 `g_adc_val[]` 进行显示和控制计算。

这说明 `9-3` 的采样功能不是临时拼接，而是形成了完整的“ADC 采集 -> 滤波 -> 偏置校正 -> 电流/温度换算 -> 显示输出”链路。

## 6. 工程配置差异

### 6.1 `Projects/MDK-ARM/atk_g474.uvprojx`

#### `9-1` 的工程配置

关键位置：

- `Projects/MDK-ARM/atk_g474.uvprojx:343` 包含 `..\..\User\Vofa\Inc`
- `Projects/MDK-ARM/atk_g474.uvprojx:573` 到 `585` 包含 `vofa_port.c`、`vofa_uart.c`、`vofa_motor.c`

说明：

- `9-1` 编译链显式纳入了 VOFA 相关头文件和源文件。

#### `9-3` 的工程配置

关键位置：

- `Projects/MDK-ARM/atk_g474.uvprojx:523` / `528` 加入 `stm32g4xx_hal_adc.c` 和 `stm32g4xx_hal_adc_ex.c`
- `Projects/MDK-ARM/atk_g474.uvprojx:568` 加入 `adc.c`

说明：

- `9-3` 删除了 VOFA 依赖。
- `9-3` 新增了 HAL ADC 驱动和自定义 ADC BSP 文件。

### 6.2 `readme.txt`

`readme.txt` 的实验名称和实验目的从：

- `BLDC基本驱动 实验`

切换成：

- `BLDC电压电流温度采集 实验`

同时硬件资源说明里新增了模拟采集接口说明，实验现象里新增了“串口与屏幕显示电压、温度、电流信息”。

这和源码层面的变化完全一致，说明 `9-3` 的实验目标已经从“能转”扩展到“能测”。

## 7. 可归纳的功能演进路径

从 `9-1` 到 `9-3` 的演进路径可以概括为：

1. 保留 BLDC 六步换相和按键控制基础。
2. 删除 PWM 测试与 VOFA 调试链路。
3. 增加 ADC1 多通道 DMA 连续采样。
4. 增加温度换算与电压换算公式。
5. 增加停机偏置校准与运行态相电流计算。
6. 增加母线电流估算。
7. 将采样结果输出到 LCD 与串口。

## 8. 如果你要把 `9-3` 的能力移植到当前工程，最少需要迁移的内容

建议至少迁移下面几部分：

1. `Drivers/BSP/ADC/adc.c` 与 `Drivers/BSP/ADC/adc.h`
2. `bldc.h` 中的 `ADC2CURT`、`ADC2VBUS`、`FirstOrderRC_LPF`、`get_temp()` 声明
3. `bldc.c` 中的温度计算函数
4. `bldc_tim.h` 中的基本定时器定义
5. `bldc_tim.c` 中的：
   - TIM6 初始化
   - 停机偏置采样
   - 运行态相电流计算
   - 母线电流计算
6. `main.c` 中的：
   - `btim_timx_int_init()`
   - `adc_dma_init()`
   - LCD/串口采样数据显示逻辑
7. `atk_g474.uvprojx` 中 ADC 源文件与 HAL ADC 驱动项

如果还想保留 `9-1` 的 VOFA 功能，则需要把 `User/Vofa` 目录和工程配置一起保留，避免在移植 `9-3` 时把上位机调试链路丢掉。

## 9. 最终结论

`9-1` 和 `9-3` 的核心区别，不在于电机是否还能转，而在于工程是否具备“测量能力”。

- `9-1` 重点是：基础驱动、波形检查、PWM 调试、VOFA 状态输出。
- `9-3` 重点是：在基础驱动上增加 ADC 采样、温度计算、三相电流与母线电流测量，并把结果显示到 LCD 和串口。

因此，如果你的目标是后续继续做速度环、电流环、保护策略、无感控制，那么 `9-3` 是更接近后续实验链路的基础版本。

## 10. 补充：`9-3 -> 9-4` 功能演进结论

### 10.1 结论

`9-4` 是在 `9-3` 的“可测量版本”基础上，进一步引入“可闭环调速能力”的版本。

如果说：

- `9-3` 解决的是“电机状态能不能被测出来”
- 那么 `9-4` 解决的是“电机速度能不能按目标值自动调上去或降下来”

### 10.2 关键演进点

#### 1. 新增速度环 PID 模块

`9-4` 相比 `9-3` 新增：

- `Drivers/BSP/PID/pid.c`
- `Drivers/BSP/PID/pid.h`

对应工程文件中也新增：

- `Projects/MDK-ARM/atk_g474.uvprojx` 中加入 `pid.c`

这说明 `9-4` 首次把速度闭环控制正式纳入工程，而不是继续停留在手动调 PWM 占空比阶段。

#### 2. 新增上位机 PID 调试链路

`9-4` 还新增：

- `Middlewares/DEBUG/debug.c`
- `Middlewares/DEBUG/debug.h`

对应工程文件中也新增：

- `Projects/MDK-ARM/atk_g474.uvprojx` 中加入 `debug.c`

这表示 `9-4` 不只是“加了 PID”，而是同时加入了参数在线观察和调参能力，便于实时调速度环参数。

#### 3. 从“直接调 PWM”变成“设定目标转速”

在 `9-4` 的 `User/main.c` 中：

- `User/main.c:60` 初始化 `pid_init()`
- `User/main.c:63` 初始化 `debug_init()`
- `User/main.c:67` 上传速度环 PID 初值到上位机
- `User/main.c:83` 显示 `SetSpeed`
- `User/main.c:85` 显示 `M1 Speed`

按键逻辑也变成：

- `KEY0`：增加目标转速
- `KEY1`：减小目标转速
- `KEY2`：停机

而不是像 `9-3` 那样直接增减 PWM 占空比。

这代表控制思路已经从“开环给定”切换到“闭环给目标值”。

#### 4. 新增霍尔测速与速度计算

`9-4` 的 `Drivers/BSP/BLDC/bldc.h` 增加了：

- `SPEED_COEFF`
- `uemf_edge(uint8_t val);`

关键位置：

- `Drivers/BSP/BLDC/bldc.h:78` `SPEED_COEFF`
- `Drivers/BSP/BLDC/bldc.h:111` `uemf_edge()`
- `Drivers/BSP/BLDC/bldc.c:317` 实现 `uemf_edge()`

这说明 `9-4` 已经把霍尔边沿信息进一步用于测速，而不仅仅用于六步换相。

#### 5. 速度环真正落在 PWM 中断里闭环执行

`9-4` 的 `Drivers/BSP/TIMER/bldc_tim.c` 是最核心的演进点：

- `Drivers/BSP/TIMER/bldc_tim.c:256` 开始做霍尔周期计数
- `Drivers/BSP/TIMER/bldc_tim.c:262` / `264` 根据计数换算转速
- `Drivers/BSP/TIMER/bldc_tim.c:265` 对速度做一阶滤波
- `Drivers/BSP/TIMER/bldc_tim.c:286` 调用 `increment_pid_ctrl(&g_speed_pid, g_bldc_motor1.speed)`
- `Drivers/BSP/TIMER/bldc_tim.c:287` 对 PID 输出再做一阶滤波
- `Drivers/BSP/TIMER/bldc_tim.c:290` / `294` 把结果转换为 `pwm_duty`

也就是说，`9-4` 把控制链路变成了：

`目标速度 -> 霍尔测速 -> 速度误差 -> PID -> PWM占空比`

### 10.3 `9-3 -> 9-4` 的一句话总结

`9-3` 解决“测得出来”，`9-4` 解决“调得住”。

它把工程从“带采样显示的基础驱动”推进成了“带测速反馈和上位机调参的速度闭环控制系统”。

## 11. 补充：`9-4 -> 9-5` 功能演进结论

### 11.1 结论

`9-5` 不是简单把 `9-4` 的 PID 参数再调一遍，而是把单一速度环升级成“速度环 + 电流环”的双环控制。

如果说：

- `9-4` 关注的是“速度能否跟踪目标”
- 那么 `9-5` 关注的是“在跟踪速度的同时，能否限制电流、兼顾力矩和保护”

### 11.2 关键演进点

#### 1. 没有新增目录，但核心控制逻辑明显升级

`9-4 -> 9-5` 没有新增文件目录，说明 `9-5` 不是靠“再挂一堆模块”实现，而是在已有 `PID`、`bldc_tim`、`main.c` 基础上升级控制算法。

这类变化比“加文件”更重要，因为它意味着控制结构发生了变化。

#### 2. PID 模块从单环扩展为双环

`9-5` 的 `Drivers/BSP/PID/pid.h` / `pid.c` 新增了：

- 电流环参数 `C_KP / C_KI / C_KD`
- `g_current_pid`
- `Up / Ui / Ud`
- `LIMIT_OUT(...)`

关键位置：

- `Drivers/BSP/PID/pid.c:28` 定义 `g_current_pid`
- `Drivers/BSP/PID/pid.c:40` 设置电流环目标值 `2500.0`
- `Drivers/BSP/PID/pid.c:48` 到 `51` 设置电流环积分和输出限制
- `Drivers/BSP/PID/pid.c:80` 到 `84` 使用 `Ui` 和 `LIMIT_OUT` 做限幅

这说明 `9-5` 已经从 `9-4` 的单个速度 PID，演进成两个 PID 同时参与控制：

- 外环：速度环
- 内环：电流环

#### 3. 上位机调试从 1 组 PID 扩展为 2 组 PID

在 `9-5` 的 `User/main.c` 中：

- `User/main.c:67` 上报 `TYPE_PID1` 给电流环
- `User/main.c:68` 上报 `TYPE_PID2` 给速度环
- `User/main.c:198` 到 `200` 分别接收两组 PID 参数
- `User/main.c:196` 把目标电流放到波形通道 7

这表示 `9-5` 的上位机调试维度已经从“只调速度环”变成“同时调速度环和电流环”。

#### 4. 双环控制核心落在 `bldc_tim.c`

`9-5` 的 `Drivers/BSP/TIMER/bldc_tim.c` 新增了一组明显的双环控制变量：

- `motor_pwm_c`
- `motor_pwm_sl`
- `pid_s_count`
- `pid_c_count`
- `cf_count`

关键位置：

- `Drivers/BSP/TIMER/bldc_tim.c:223` `motor_pwm_c`
- `Drivers/BSP/TIMER/bldc_tim.c:224` `motor_pwm_sl`
- `Drivers/BSP/TIMER/bldc_tim.c:225` `pid_s_count`
- `Drivers/BSP/TIMER/bldc_tim.c:226` `pid_c_count`
- `Drivers/BSP/TIMER/bldc_tim.c:227` `cf_count`
- `Drivers/BSP/TIMER/bldc_tim.c:356` / `363` 调用 `increment_pid_ctrl(&g_current_pid, adc_amp_bus)`
- `Drivers/BSP/TIMER/bldc_tim.c:369` 到 `383` 在速度环输出和电流环输出之间切换

从代码逻辑看，`9-5` 的控制链路变成了：

`目标速度 -> 速度环 -> 期望PWM/力矩需求 -> 电流环限流 -> 最终PWM占空比`

其中电流环起到“限制输出、抑制过流、提高带载稳定性”的作用。

#### 5. 电流环直接使用 `adc_amp_bus` 作为反馈

`9-5` 不是估算一个虚拟电流量，而是直接复用了 `9-3` / `9-4` 已经建立的电流采样链路：

- `Drivers/BSP/TIMER/bldc_tim.c:259` 到 `279` 继续计算 `adc_amp_bus`
- `Drivers/BSP/TIMER/bldc_tim.c:350` 用 `g_current_pid.SetPoint` 与 `adc_amp_bus` 比较
- `Drivers/BSP/TIMER/bldc_tim.c:356` / `363` 以 `adc_amp_bus` 作为电流环反馈

这说明 `9-5` 的出现，正是建立在 `9-3` 电流采样能力和 `9-4` 速度闭环能力都已经具备的基础上。

#### 6. 新增堵转保护/重启处理

`9-5` 在 `Drivers/BSP/BLDC/bldc.h` 中新增：

- `LOCK_TAC`

关键位置：

- `Drivers/BSP/BLDC/bldc.h:94` 定义 `LOCK_TAC`
- `Drivers/BSP/TIMER/bldc_tim.c:397` 到 `402` 标记堵转
- `Drivers/BSP/TIMER/bldc_tim.c:436` 到 `452` 做堵转后的延时重启处理

这说明 `9-5` 不再只是“调得动”，还开始考虑“调坏了怎么办”“堵转后如何恢复”，也就是更接近实际控制系统的保护逻辑。

### 11.3 `9-4 -> 9-5` 的一句话总结

`9-4` 解决“速度闭环”，`9-5` 解决“速度闭环在带载和限流条件下仍然可控”。

它把工程从“单速度环调速实验”推进成了“速度外环 + 电流内环 + 初步保护策略”的双环控制实验。

## 12. 串起来看的总演进路径

如果把 `9-1`、`9-3`、`9-4`、`9-5` 串成一条链，功能演进可以概括为：

1. `9-1`：先让 BLDC 正常换相、正反转、调 PWM。
2. `9-3`：再把电压、温度、三相电流、母线电流测出来。
3. `9-4`：在可测基础上加入霍尔测速、速度环 PID、上位机调参。
4. `9-5`：在速度闭环基础上再加入电流内环、限流和堵转处理。

因此，这条实验链的本质推进顺序是：

`基础驱动 -> 状态测量 -> 单环闭环 -> 双环闭环`
