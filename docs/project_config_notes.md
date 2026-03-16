# 项目配置注意事项

## UART配置重要提醒

### ⚠️ 关键问题：GPIO速度配置

**问题描述：**
使用高波特率（≥921600）时，如果GPIO速度设置为LOW，会导致UART无法正常通信。

**根本原因：**
- `GPIO_SPEED_FREQ_LOW` 最大支持2MHz翻转速率
- 高波特率需要更高的GPIO翻转速率
- CubeMX默认生成LOW速度，不适合高速通信

**解决方案：**
在 `Core/Src/usart.c` 的 `HAL_UART_MspInit()` 函数中：
```c
GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;  // 必须设置为VERY_HIGH
```

**波特率与GPIO速度对应关系：**
| 波特率 | 最低GPIO速度 |
|--------|-------------|
| ≤115200 | LOW (2MHz) |
| 115200-460800 | MEDIUM (25MHz) |
| 460800-921600 | HIGH (50MHz) |
| ≥1000000 | VERY_HIGH (100MHz) |

### 检查方法

#### 方法1：手动检查
打开 `Core/Src/usart.c`，搜索 `GPIO_InitStruct.Speed`，确认为：
```c
GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
```

#### 方法2：使用检查脚本
```bash
python scripts/check_uart_config.py Core/Src/usart.c
```

### CubeMX重新生成代码后的检查清单

每次使用CubeMX重新生成代码后，必须检查：

- [ ] UART波特率是否正确
- [ ] GPIO速度是否匹配波特率
- [ ] 过采样设置是否合理
- [ ] 时钟配置是否正确

### 预防措施

1. **在usart.c中添加注释**
   ```c
   GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;  /* 高速UART必须使用VERY_HIGH */
   ```

2. **在CubeMX中配置GPIO速度**
   - Configuration → GPIO → PB6/PB7
   - Maximum output speed → Very High

3. **编译后测试**
   - 上电后立即发送测试数据
   - 确认串口能正常接收

## 其他常见配置问题

### 1. 波特率误差过大
**症状：** 串口乱码或无法通信
**原因：** 时钟配置导致波特率误差>2%
**解决：** 调整系统时钟或选择其他波特率

### 2. 线缆过长
**症状：** 高波特率下通信不稳定
**原因：** 信号衰减和干扰
**解决：** 使用短线缆（<0.5米）或降低波特率

### 3. 上位机不支持
**症状：** 设置正确但无法通信
**原因：** USB转串口芯片不支持高波特率
**解决：** 更换支持高速的转换器（如FT232RL）

## 版本记录

- 2026-03-16: 发现并修复GPIO速度配置问题
- 波特率：115200 → 1000000
- GPIO速度：LOW → VERY_HIGH
- 过采样：16倍 → 8倍
