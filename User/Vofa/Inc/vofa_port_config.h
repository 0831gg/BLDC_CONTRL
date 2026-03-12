#ifndef __VOFA_PORT_CONFIG_H
#define __VOFA_PORT_CONFIG_H

/*=============================================================================
 *  VOFA+ 串口移植配置文件（统一版）
 *
 *  ★ 移植到新芯片/新板子时，只需修改本文件 ★
 *
 *  步骤:
 *    1. 选择库类型（HAL 或 标准库）
 *    2. 选择芯片平台
 *    3. 配置USART外设及引脚
 *    4. 配置时钟频率
 *    5. 配置DMA（可选）
 *    6. 调整波特率表和功能参数
 *=============================================================================*/

/*============================ 第1步: 库类型 ================================*/
/*  1 = HAL库（CubeMX工程）
 *  0 = 标准外设库（StdPeriph）                                              */
#define VOFA_USE_HAL                1

/*========================== 第2步: 芯片平台 ================================*/
/* 取消注释你使用的平台 */
//#define VOFA_PLATFORM_STM32F4
// #define VOFA_PLATFORM_STM32F1
// #define VOFA_PLATFORM_STM32H7
 #define VOFA_PLATFORM_STM32G4

/*========================== 第3步: 头文件包含 ==============================*/
#if VOFA_USE_HAL
    /*--- HAL库: 包含 CubeMX 生成的头文件 ---*/
    #include "main.h"
    #include "usart.h"
    #ifdef VOFA_USE_DMA
    #include "dma.h"
    #endif
#else
    /*--- 标准库: 包含芯片头文件 ---*/
    #ifdef VOFA_PLATFORM_STM32F4
        #include "stm32f4xx.h"
    #elif defined(VOFA_PLATFORM_STM32F1)
        #include "stm32f10x.h"
    #elif defined(VOFA_PLATFORM_STM32H7)
        #include "stm32h7xx.h"
    #endif
#endif

/*========================= 第4步: USART外设选择 ============================*/
/*
 * 高波特率建议:
 *   STM32F4: USART1 或 USART6（挂 APB2，时钟 84MHz）
 *   STM32F1: USART1（挂 APB2，时钟 72MHz）
 *   STM32H7: USART1（APB2，可达 100MHz）
 */
#if VOFA_USE_HAL
    /* HAL库: 填写 CubeMX 生成的句柄名(usart.h中定义) */
    #define VOFA_HUART              huart1
#else
    /* 标准库: 填写 USART 外设基地址 */
    #define VOFA_USARTx             USART1
#endif

#define VOFA_USART_IRQn             USART1_IRQn

/*========================= 第5步: 时钟配置 =================================*/
/*
 * ★ 这是波特率计算的关键参数，必须与实际系统时钟一致 ★
 *
 *  USART1/6 → APB2    USART2/3/4/5 → APB1
 *  -----------------------------------------------
 *  F407: APB2=84MHz   APB1=42MHz   (SYSCLK=168MHz)
 *  F103: APB2=72MHz   APB1=36MHz   (SYSCLK=72MHz)
 *  H743: APB2=100MHz  APB1=100MHz  (SYSCLK=400MHz)
 */
#define VOFA_USART_PCLK_HZ         168000000UL

/*==================== 第6步: 标准库专用 - GPIO配置 =========================*/
/* HAL库用户跳过此节，CubeMX已自动配置GPIO */
#if !VOFA_USE_HAL

    /* --- USART时钟 --- */
    #ifdef VOFA_PLATFORM_STM32F4
        #define VOFA_USART_CLK          RCC_APB2Periph_USART1
        #define VOFA_USART_CLK_CMD      RCC_APB2PeriphClockCmd
    #elif defined(VOFA_PLATFORM_STM32F1)
        #define VOFA_USART_CLK          RCC_APB2Periph_USART1
        #define VOFA_USART_CLK_CMD      RCC_APB2PeriphClockCmd
    #endif

    /* --- TX引脚 --- */
    #define VOFA_TX_PORT                GPIOA
    #define VOFA_TX_PIN                 GPIO_Pin_9

    #ifdef VOFA_PLATFORM_STM32F4
        #define VOFA_TX_GPIO_CLK        RCC_AHB1Periph_GPIOA
        #define VOFA_TX_GPIO_CLK_CMD    RCC_AHB1PeriphClockCmd
        #define VOFA_TX_PINSOURCE       GPIO_PinSource9
        #define VOFA_TX_AF              GPIO_AF_USART1
    #elif defined(VOFA_PLATFORM_STM32F1)
        #define VOFA_TX_GPIO_CLK        RCC_APB2Periph_GPIOA
        #define VOFA_TX_GPIO_CLK_CMD    RCC_APB2PeriphClockCmd
    #endif

    /* --- RX引脚 --- */
    #define VOFA_RX_PORT                GPIOA
    #define VOFA_RX_PIN                 GPIO_Pin_10

    #ifdef VOFA_PLATFORM_STM32F4
        #define VOFA_RX_GPIO_CLK        RCC_AHB1Periph_GPIOA
        #define VOFA_RX_GPIO_CLK_CMD    RCC_AHB1PeriphClockCmd
        #define VOFA_RX_PINSOURCE       GPIO_PinSource10
        #define VOFA_RX_AF              GPIO_AF_USART1
    #elif defined(VOFA_PLATFORM_STM32F1)
        #define VOFA_RX_GPIO_CLK        RCC_APB2Periph_GPIOA
        #define VOFA_RX_GPIO_CLK_CMD    RCC_APB2PeriphClockCmd
    #endif

#endif /* !VOFA_USE_HAL */

/*========================= 第7步: DMA配置（可选）==========================*/
#define VOFA_USE_DMA                0   /* 1=DMA发送, 0=轮询发送 */

#if VOFA_USE_DMA && !VOFA_USE_HAL
    /* 标准库DMA配置（HAL库由CubeMX配置，无需此处设置） */
    #ifdef VOFA_PLATFORM_STM32F4
        #define VOFA_DMA_STREAM         DMA2_Stream7
        #define VOFA_DMA_CHANNEL        DMA_Channel_4
        #define VOFA_DMA_CLK            RCC_AHB1Periph_DMA2
        #define VOFA_DMA_CLK_CMD        RCC_AHB1PeriphClockCmd
        #define VOFA_DMA_FLAG_TC        DMA_FLAG_TCIF7
        #define VOFA_DMA_IRQn           DMA2_Stream7_IRQn
    #endif
#endif

/*========================= 第8步: 功能参数 ================================*/
/* 过采样支持: F4/H7/G4=1,  F1/F0=0 */
#ifdef VOFA_PLATFORM_STM32F4
    #define VOFA_SUPPORT_OVER8      1
#elif defined(VOFA_PLATFORM_STM32H7)
    #define VOFA_SUPPORT_OVER8      1
#elif defined(VOFA_PLATFORM_STM32G4)
    #define VOFA_SUPPORT_OVER8      1
#else
    #define VOFA_SUPPORT_OVER8      0
#endif

/* 波特率误差阈值(%)，超过则降级，推荐 <= 2.0 */
#define VOFA_MAX_BAUD_ERROR         2.0f

/* 波特率降级表（从高到低），按需增删 */
/* CH340C: 先用115200验证，通过后可改为 {2000000, 921600, 460800, 230400, 115200} */
#define VOFA_BAUD_TABLE             {2000000}
#define VOFA_BAUD_TABLE_SIZE        1

/* 发送缓冲区大小(字节) */
#define VOFA_TX_BUF_SIZE            512

/* 验证测试使能 */
#define VOFA_ENABLE_TEST            1   /* 1=编译测试代码, 0=不编译 */

#endif /* __VOFA_PORT_CONFIG_H */
