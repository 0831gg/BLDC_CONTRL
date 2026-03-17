# UART 高波特率与 VOFA 专用推荐配置

## 1. 工程基线与适用范围

本文档只针对当前项目，不作为通用 STM32 串口教程。

- MCU：`STM32G474VETx`
- 外部晶振：`HSE = 25MHz`
- 主系统时钟：`SYSCLK = 170MHz`
- UART 口：`USART1`
- 引脚：`PB6 = TX`，`PB7 = RX`
- 当前生成代码：`115200 / 8N1 / 无流控 / OverSampling=16`
- 当前 `USART1 Clock Mux`：`HSI16`
- 当前 VOFA 发送链路：`VOFA FireWater -> huart1`
- 当前 VOFA 发送模式：阻塞发送，`VOFA_USE_DMA = 0`

当前仓库中的 VOFA 语义也需要先说明清楚：

- 当前代码协议是 `FireWater`，不是 `RawData`
- 当前测试入口发送的是多通道数值，不是普通文本日志
- 当前 `test.c` 中每 `50ms` 发送一次波形数据
- 当前 `VOFA` 底层绑定的是 `huart1`

## 2. 三套推荐配置总表

| 场景 | 推荐目标 | CubeMX 波特率 | USART1 Clock Mux | OverSampling | GPIO Speed | DMA 建议 | 说明 |
|------|-----------|----------------|------------------|--------------|------------|----------|------|
| `1M 稳定优先` | 稳定性优先 | `1000000` | `HSI16` | `16` | `Very High` | 可不启用 | 整数分频，最稳 |
| `2M 速度优先` | 带宽优先 | `2000000` | `SYSCLK` | `16` | `Very High` | 建议启用 `USART1_TX DMA` | 适合更高刷新率 |
| `VOFA 专用推荐配置` | 当前项目可视化优先 | `1000000` | `HSI16` | `16` | `Very High` | 默认可不启用 | 适合当前 `FireWater` 数值输出 |

三套方案都固定采用以下基础参数：

- `Mode = Asynchronous`
- `Word Length = 8 Bits`
- `Parity = None`
- `Stop Bits = 1`
- `Hardware Flow Control = None`
- `Clock Prescaler = DIV1`

## 3. 1M 稳定优先 CubeMX 配置步骤

### 3.1 推荐配置

在 CubeMX 中将 `USART1` 配置为：

- `Baud Rate = 1000000`
- `Mode = Asynchronous`
- `Word Length = 8 Bits`
- `Parity = None`
- `Stop Bits = 1`
- `Hardware Flow Control = None`
- `Clock Prescaler = DIV1`
- `OverSampling = 16`
- `USART1 Clock Mux = HSI16`
- `PB6 / PB7 GPIO Speed = Very High`

### 3.2 为什么 1M 选 HSI16

这是当前项目里最稳的一档高速串口方案，原因很直接：

- `HSI16 / 1,000,000 = 16`
- 分频结果是整数
- 配合 `16x OverSampling`，时钟关系简单，误差控制最容易
- 即使主系统仍跑在 `170MHz PLL`，串口内核时钟也不依赖主 PLL 的细节变化

所以如果你的目标是“高速但先求稳定”，当前项目优先选：

- `1M`
- `HSI16`
- `OverSampling = 16`

### 3.3 CubeMX 页面操作建议

#### `Connectivity -> USART1`

- 选择 `Asynchronous`
- 将 `Baud Rate` 改为 `1000000`
- 将 `OverSampling` 保持为 `16`
- 将 `Clock Prescaler` 设为 `DIV1`

#### `System Core -> RCC`

- 找到 `USART1 Clock Mux`
- 选择 `HSI16`

#### `System Core -> GPIO`

- 找到 `PB6` 和 `PB7`
- 保持 `Alternate Function Push Pull`
- `Pull = No pull-up and no pull-down`
- 将 `GPIO Speed` 设为 `Very High`

### 3.4 适用场景

这套配置适合：

- 需要把串口波特率从 `115200` 提升到 `1M`
- 需要先验证硬件链路稳定性
- 需要用 VOFA 做数值波形显示，但还不想先引入 DMA 变量

## 4. 2M 速度优先 CubeMX 配置步骤

### 4.1 推荐配置

在 CubeMX 中将 `USART1` 配置为：

- `Baud Rate = 2000000`
- `Mode = Asynchronous`
- `Word Length = 8 Bits`
- `Parity = None`
- `Stop Bits = 1`
- `Hardware Flow Control = None`
- `Clock Prescaler = DIV1`
- `OverSampling = 16`
- `USART1 Clock Mux = SYSCLK`
- `PB6 / PB7 GPIO Speed = Very High`
- `DMA Settings -> Add -> USART1_TX`

### 4.2 为什么 2M 选 SYSCLK

在当前项目里，`SYSCLK = 170MHz`，因此：

- `170,000,000 / 2,000,000 = 85`
- 对 `2M` 来说，`SYSCLK` 更适合继续保持 `16x OverSampling`
- 这样既能拿到更高带宽，也不需要把 `2M` 降成 `8x` 过采样主方案

因此当前工程下，`2M` 的主推荐不是 `HSI16`，而是：

- `2M`
- `SYSCLK`
- `OverSampling = 16`

### 4.3 不推荐作为主方案的组合

当前项目里，不推荐把以下组合作为 `2M` 主方案：

- `2M + HSI16 + OverSampling = 16`

原因是：

- `HSI16` 本身只有 `16MHz`
- 对 `2Mbps` 来说余量过小
- 这不适合作为“速度优先”的主推荐方案

`OverSampling = 8` 可以作为备选说明，但不作为本项目推荐表主配置。

### 4.4 CubeMX 页面操作建议

#### `Connectivity -> USART1`

- 选择 `Asynchronous`
- 将 `Baud Rate` 改为 `2000000`
- 将 `OverSampling` 设为 `16`
- 将 `Clock Prescaler` 设为 `DIV1`

#### `System Core -> RCC`

- 找到 `USART1 Clock Mux`
- 选择 `SYSCLK`

#### `System Core -> GPIO`

- 找到 `PB6` 和 `PB7`
- 将 `GPIO Speed` 设为 `Very High`

#### `DMA Settings`

- 添加 `USART1_TX`
- `Direction = Memory To Peripheral`
- `Mode = Normal`
- `Periph/Mem Data Width = Byte`
- `Mem Inc = Enable`

### 4.5 适用场景

这套配置适合：

- 需要更高串口刷新率
- 需要在 VOFA 中提高通道数或刷新频率
- 需要同时混合数值输出和较多文本输出
- 希望把串口发送对主循环的阻塞时间进一步压缩

## 5. VOFA 专用推荐配置

### 5.1 当前项目默认推荐

对于当前仓库，VOFA 专用推荐配置固定为：

- `Baud Rate = 1000000`
- `USART1 Clock Mux = HSI16`
- `OverSampling = 16`
- `Mode = Asynchronous`
- `8N1`
- `无校验`
- `无流控`
- `PB6 / PB7 = Very High`
- VOFA 协议选择：`FireWater`

### 5.2 为什么默认推荐 1M 而不是 2M

当前仓库的 VOFA 发送特点是：

- 当前发送的是 `FireWater` 文本数值
- 当前测试入口是 8 通道数据
- 当前发送周期是 `50ms`
- 当前底层发送模式仍是阻塞发送

对这组参数来说，默认推荐先用：

- `1M + HSI16 + FireWater`

原因是这套组合已经足够覆盖当前的可视化测试需求，同时更容易先把链路稳定性跑通。

### 5.3 当前代码链路说明

当前工程不是 `RawData` 测试链路，而是：

- `Test_Init()` 中执行 `VOFA_Init()`
- `Test_Loop()` 中每 `50ms` 调用发送函数
- 通过 `VOFA_FireWaterFloat()` 发送多通道数值
- 通过 `HAL_UART_Transmit()` 从 `huart1` 发出

因此上位机必须按当前代码语义设置，而不是按其他协议猜测：

- 串口选择：`USART1` 对应的 `COM`
- 协议：`FireWater`
- 参数：`8N1 / 无校验 / 无流控`

### 5.4 什么时候切换到 2M + SYSCLK + DMA

当出现以下目标时，再切换到更激进的高速方案：

- VOFA 刷新频率明显提高
- 通道数明显增加
- 同时混入大量文本输出
- 阻塞发送开始影响主循环实时性

此时推荐改为：

- `2M`
- `SYSCLK`
- `OverSampling = 16`
- 开启 `USART1_TX DMA`

## 6. 生成代码后的核对清单与常见误区

### 6.1 Generate Code 后必须核对的项目

#### `.ioc` 中检查

重点检查以下字段：

- `USART1.BaudRate`
- `RCC.USART1CLockSelection`
- `PB6.GPIO_Speed`
- `PB7.GPIO_Speed`
- 若采用高速 VOFA 方案，检查 `DMA Settings -> USART1_TX` 是否仍保留

#### `usart.c` 中检查

重点检查以下生成代码：

- `huart1.Init.BaudRate`
- `huart1.Init.OverSampling`
- `PeriphClkInit.Usart1ClockSelection`
- `GPIO_InitStruct.Speed`
- `hdma_usart1_tx.Init.Request = DMA_REQUEST_USART1_TX`

### 6.2 当前工程的已生效配置

截至当前代码，仓库中实际生效的是：

- `.ioc` 中 `USART1.BaudRate = 115200`
- `.ioc` 中 `RCC.USART1CLockSelection = RCC_USART1CLKSOURCE_HSI`
- `usart.c` 中 `huart1.Init.BaudRate = 115200`
- `usart.c` 中 `huart1.Init.OverSampling = UART_OVERSAMPLING_16`
- `usart.c` 中 `PeriphClkInit.Usart1ClockSelection = RCC_USART1CLKSOURCE_HSI`
- `PB6 / PB7` 当前生成代码里仍是 `GPIO_SPEED_FREQ_HIGH`

也就是说，本文档里的 `1M / 2M` 方案是“目标推荐配置”，不是当前代码已经切换完成的状态。

### 6.3 常见误区

#### 误区 1：继续沿用旧结论“2 Mbps 在当前工程下不可用”

这个结论不能继续直接套用到当前项目。

在当前项目里：

- `SYSCLK = 170MHz`
- `2Mbps` 方案可以通过 `USART1 Clock Mux = SYSCLK` 来实现
- 主推荐应基于当前时钟结构重新判断，而不是沿用旧文档结论

#### 误区 2：默认把 PCLK2 当作唯一推荐串口时钟源

当前项目不应把 `PCLK2` 当作唯一默认推荐。

在 CubeMX 中，`USART1 Clock Mux` 是独立可选的：

- 你可以选 `HSI16`
- 也可以选 `SYSCLK`

当前项目中：

- `1M 稳定优先` 推荐 `HSI16`
- `2M 速度优先` 推荐 `SYSCLK`

#### 误区 3：认为当前代码已经跑在 1Mbps

当前仓库并没有已经切到 `1Mbps`。

当前代码实际还是：

- `115200`
- `HSI16`
- `OverSampling = 16`

如果要切换到本文档中的 `1M` 或 `2M`，仍需要在 CubeMX 中改配置并重新生成代码。

#### 误区 4：把当前 VOFA 默认协议当成 RawData

当前仓库 VOFA 默认语义不是 `RawData`，而是 `FireWater`。

如果上位机协议选错，即使串口链路正常，也会表现为：

- 无法正确显示波形
- 数据看起来像异常文本
- 上位机无正常刷新

## 附：推荐执行顺序

如果你现在准备做高速串口验证，建议按下面顺序推进：

1. 先切到 `1M 稳定优先`
2. 用串口助手或 VOFA 验证链路稳定
3. 再根据刷新率需求决定是否升级到 `2M 速度优先`
4. 如果进入高刷新率 VOFA 阶段，再打开 `USART1_TX DMA`

这样最容易定位问题，也最不容易把“时钟问题、协议问题、DMA问题”混在一起。
