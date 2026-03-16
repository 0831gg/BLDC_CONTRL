# 代码注释规范

## 文档信息
- 项目：BLDC电机控制系统
- 版本：v1.0
- 日期：2026-03-16

---

## 1. 文件头注释规范

### 1.1 头文件（.h）

```c
/**
 * @file    文件名.h
 * @brief   模块简要说明（一句话）
 * @details 详细说明模块功能、使用场景、依赖关系
 * @author  作者名（可选）
 * @date    创建日期（可选）
 * @version 版本号（可选）
 *
 * @note    重要说明或使用注意事项
 * @warning 警告信息
 * @see     相关文件或参考资料
 *
 * @example 使用示例（可选）
 * @code
 * // 示例代码
 * @endcode
 */
```

### 1.2 源文件（.c）

```c
/**
 * @file    文件名.c
 * @brief   模块实现
 * @details 实现文件的详细说明
 * @see     对应的头文件
 */
```

---

## 2. 函数注释规范

### 2.1 标准格式

```c
/**
 * @brief  函数简要说明（一句话，动词开头）
 * @param  参数名: 参数说明
 * @param  参数名: 参数说明
 * @retval 返回值说明
 *
 * @note   使用说明或注意事项
 * @warning 警告信息
 *
 * @example 使用示例（可选）
 * @code
 * // 示例代码
 * @endcode
 */
```

### 2.2 参数说明规范

- **输入参数**：`@param 参数名: 说明`
- **输出参数**：`@param[out] 参数名: 说明`
- **输入输出参数**：`@param[in,out] 参数名: 说明`
- **返回值**：`@retval 说明` 或 `@return 说明`

### 2.3 返回值说明

```c
/**
 * @retval 0: 成功
 * @retval 1: 失败
 * @retval 2: 超时
 */

// 或者

/**
 * @return 实际读取的字节数，失败返回-1
 */
```

---

## 3. 变量注释规范

### 3.1 全局变量

```c
/** @brief 变量简要说明 */
static uint32_t s_variable_name = 0;

/**
 * @brief 复杂变量的详细说明
 * @note  使用注意事项
 */
static volatile uint8_t s_complex_variable = 0;
```

### 3.2 结构体

```c
/**
 * @brief 结构体简要说明
 */
typedef struct {
    uint32_t field1;  /**< 字段1说明 */
    uint16_t field2;  /**< 字段2说明 */
    uint8_t  field3;  /**< 字段3说明 */
} StructName_t;
```

### 3.3 枚举

```c
/**
 * @brief 枚举类型说明
 */
typedef enum {
    ENUM_VALUE_1 = 0,  /**< 值1说明 */
    ENUM_VALUE_2 = 1,  /**< 值2说明 */
    ENUM_VALUE_3 = 2   /**< 值3说明 */
} EnumName_t;
```

---

## 4. 宏定义注释规范

### 4.1 简单宏

```c
/** @brief 宏简要说明 */
#define MACRO_NAME  100

/** @brief 计算公式说明 */
#define CALC_VALUE(x)  ((x) * 2 + 1)
```

### 4.2 复杂宏

```c
/**
 * @brief 宏功能说明
 * @param x: 参数说明
 * @return 返回值说明
 * @note 使用注意事项
 */
#define COMPLEX_MACRO(x, y)  \
    do {                     \
        /* 实现 */           \
    } while(0)
```

---

## 5. 代码块注释规范

### 5.1 分隔注释

```c
/*=============================================================================
 *  模块名称
 *=============================================================================*/

/*-----------------------------------------------------------------------------
 *  子模块名称
 *-----------------------------------------------------------------------------*/
```

### 5.2 行内注释

```c
uint32_t value = 100;  /* 简短说明 */

/* 较长的说明可以单独一行 */
uint32_t another_value = 200;
```

### 5.3 TODO/FIXME/NOTE

```c
/* TODO: 待实现的功能 */
/* FIXME: 需要修复的问题 */
/* NOTE: 重要说明 */
/* WARNING: 警告信息 */
```

---

## 6. 特殊注释标记

### 6.1 Doxygen标记

| 标记 | 说明 | 示例 |
|------|------|------|
| @brief | 简要说明 | @brief 初始化UART |
| @details | 详细说明 | @details 配置波特率、数据位等 |
| @param | 参数说明 | @param data: 数据指针 |
| @retval | 返回值 | @retval 0: 成功 |
| @return | 返回值 | @return 实际字节数 |
| @note | 注意事项 | @note 调用前需初始化 |
| @warning | 警告 | @warning 不可重入 |
| @see | 参考 | @see bsp_uart.h |
| @code | 代码示例开始 | @code |
| @endcode | 代码示例结束 | @endcode |

### 6.2 自定义标记

```c
/**
 * @hardware STM32G474
 * @frequency 168MHz
 * @interrupt TIM5_IRQHandler
 * @dma DMA1_Channel1
 */
```

---

## 7. 模块化注释示例

### 7.1 BSP层示例

```c
/**
 * @file    bsp_uart.h
 * @brief   UART调试通信接口
 * @details 提供UART1的初始化和通信功能，支持DMA和阻塞两种模式
 *
 * @note    硬件配置：
 *          - USART1: PB6(TX), PB7(RX)
 *          - 波特率: 115200 (可配置)
 *          - DMA: DMA1_Channel1 (可选)
 *
 * @note    使用方法：
 *          1. 调用 BSP_UART_Init() 初始化
 *          2. 调用 BSP_UART_Printf() 打印信息
 *
 * @warning DMA模式需要在CubeMX中配置USART1 DMA TX
 *
 * @see     bsp_uart.c
 */
```

### 7.2 Motor层示例

```c
/**
 * @file    motor_ctrl.h
 * @brief   BLDC电机控制接口
 * @details 实现BLDC电机的启动、停止、速度控制等功能
 *
 * @note    控制方式：六步换相
 * @note    反馈方式：霍尔传感器
 * @note    PWM频率：10kHz
 *
 * @see     motor_ctrl.c, motor_phase_tab.h
 */
```

---

## 8. 注释质量标准

### 8.1 必须注释的内容

- ✅ 所有公共API函数
- ✅ 所有头文件
- ✅ 复杂的算法和逻辑
- ✅ 硬件相关的配置
- ✅ 中断处理函数
- ✅ 全局变量和静态变量

### 8.2 可选注释的内容

- 简单的getter/setter函数
- 显而易见的代码
- 临时调试代码

### 8.3 注释质量要求

**好的注释**：
```c
/**
 * @brief  计算电机转速（RPM）
 * @param  period_ticks: 换相周期（TIM5计数值）
 * @retval 转速（RPM），0表示电机停止
 *
 * @note   计算公式：RPM = (TIM5频率 × 60) / (周期 × 6)
 *         其中6是BLDC电机每转的换相次数
 */
uint32_t calculate_rpm(uint32_t period_ticks);
```

**不好的注释**：
```c
// 计算RPM
uint32_t calculate_rpm(uint32_t period_ticks);
```

---

## 9. 中文注释规范

### 9.1 何时使用中文

- ✅ 文件头说明
- ✅ 复杂算法的详细解释
- ✅ 硬件配置说明
- ✅ 使用示例和注意事项

### 9.2 何时使用英文

- ✅ 函数名、变量名
- ✅ Doxygen标记（@brief, @param等）
- ✅ 简短的行内注释

### 9.3 混合使用示例

```c
/**
 * @brief  初始化霍尔传感器
 * @retval 0: 成功, 1: 失败
 *
 * @note   硬件连接：
 *         - Hall U: PA0
 *         - Hall V: PA1
 *         - Hall W: PA2
 *
 * @note   采样方式：TIM5定时器50kHz轮询
 */
uint8_t bsp_hall_init(void);
```

---

## 10. 注释维护规范

### 10.1 更新时机

- 修改函数接口时，必须同步更新注释
- 修改算法逻辑时，必须更新相关注释
- 添加新功能时，必须添加完整注释

### 10.2 注释审查

- 代码审查时检查注释完整性
- 定期检查注释与代码的一致性
- 删除过时的注释

### 10.3 版本控制

```c
/**
 * @file    module.h
 * @brief   模块说明
 * @version 1.0 - 2026-03-15 - 初始版本
 * @version 1.1 - 2026-03-16 - 添加DMA支持
 */
```

---

## 11. 工具支持

### 11.1 Doxygen配置

本项目支持Doxygen自动生成文档，配置文件：`Doxyfile`

生成文档命令：
```bash
doxygen Doxyfile
```

### 11.2 IDE支持

- **Keil MDK**: 支持Doxygen风格注释
- **VS Code**: 安装Doxygen插件自动生成注释模板

---

## 12. 示例模板

### 12.1 完整头文件模板

参见：`docs/templates/header_template.h`

### 12.2 完整源文件模板

参见：`docs/templates/source_template.c`

---

## 附录：常用缩写

| 缩写 | 全称 | 中文 |
|------|------|------|
| BSP | Board Support Package | 板级支持包 |
| HAL | Hardware Abstraction Layer | 硬件抽象层 |
| DMA | Direct Memory Access | 直接内存访问 |
| PWM | Pulse Width Modulation | 脉宽调制 |
| ADC | Analog-to-Digital Converter | 模数转换器 |
| UART | Universal Asynchronous Receiver/Transmitter | 通用异步收发器 |
| RPM | Revolutions Per Minute | 每分钟转速 |
| BLDC | Brushless DC Motor | 无刷直流电机 |
