/**
 * @file    test_phase0.c
 * @brief   Phase 0 基础硬件自检测试
 *
 * 测试项目:
 *   1. BSP_UART_Printf: 打印 "System Ready"，验证 USART1 DMA TX 链路
 *   2. Vofa 数据发送: 发送正弦波测试数据
 *   3. LED0 闪烁: 500ms 翻转，证明主循环在运行
 *   4. 按键事件: KEY0/1/2 按下时串口打印事件
 *
 * ┌──────────────────────────────────────────────────────────────┐
 * │ 关于 Vofa 协议选择:                                          │
 * │                                                              │
 * │  ★ JustFloat 和 FireWater 不能同时使用！★                    │
 * │                                                              │
 * │  JustFloat:  二进制浮点帧，适合高速波形显示                     │
 * │    - 调用: VOFA_JustFloat(data, ch_count)                    │
 * │    - Vofa上位机协议选: JustFloat                              │
 * │    - 优点: 带宽利用率高，实时性好                               │
 * │    - 缺点: 不能同时打印文字                                    │
 * │                                                              │
 * │  FireWater:  文本格式 "v1,v2,...\n"，适合调试+波形             │
 * │    - 调用: VOFA_FireWaterFloat(data, ch_count) 发波形         │
 * │    - 调用: VOFA_Printf("...") 发文字                         │
 * │    - Vofa上位机协议选: FireWater                              │
 * │    - 优点: 文字和波形可以混合                                   │
 * │    - 缺点: 带宽开销大(文本编码)，高通道/高频时可能丢帧          │
 * │                                                              │
 * │  切换方法:                                                    │
 * │    修改下方 TEST_VOFA_PROTOCOL 宏:                            │
 * │      0 = JustFloat (默认，推荐电机调试用)                      │
 * │      1 = FireWater (需要同时看文字时切这个)                    │
 * └──────────────────────────────────────────────────────────────┘
 */

#include "test_phase0.h"
#include "bsp_uart.h"
#include "bsp_led.h"
#include "bsp_key.h"
#include "bsp_delay.h"
#include "vofa_uart.h"
#include <math.h>

/*=========================== 配置区 ===========================*/

/**
 * Vofa 协议选择 (二选一，不能同时用！)
 *   0 = JustFloat  (二进制浮点，Vofa上位机选 JustFloat)
 *   1 = FireWater  (文本格式，  Vofa上位机选 FireWater)
 */
#define TEST_VOFA_PROTOCOL    0

/* Vofa 数据发送间隔(ms)，10ms = 100Hz 刷新率 */
#define TEST_VOFA_INTERVAL_MS   10

/* LED 闪烁间隔(ms) */
#define TEST_LED_INTERVAL_MS    500

/* 按键扫描间隔(ms) */
#define TEST_KEY_INTERVAL_MS    20

/*=========================== 变量 =============================*/

static KeyGroup_HandleTypeDef hkey;

/* 时间戳 */
static uint32_t tick_vofa  = 0;
static uint32_t tick_led   = 0;
static uint32_t tick_key   = 0;

/* 正弦波角度计数 */
static float angle = 0.0f;

/*=========================== 初始化 ===========================*/

void Test_Phase0_Init(void)
{
    /* 1. DWT 延时初始化 */
    Delay_Init();

    /* 2. BSP UART 初始化 (设置 DMA 发送完成标志) */
    BSP_UART_Init();

    /* 3. LED 初始化 */
    LED_Init();

    /* 4. 按键初始化 (长按阈值 1s) */
    Key_Init(&hkey, KEY_DEFAULT_LONG_PRESS_TIME);

    /* 5. Vofa 初始化 (自动选择最高可用波特率) */
    VOFA_Init();

    /* 6. 打印启动信息 (FireWater时才发送文本, JustFloat模式不能混入文本) */
#if (TEST_VOFA_PROTOCOL != 0)
    BSP_UART_Printf("=============================\r\n");
    BSP_UART_Printf("  Phase 0: System Ready\r\n");
    BSP_UART_Printf("  SYSCLK : 168 MHz\r\n");
    BSP_UART_Printf("  USART1 : 115200 bps\r\n");
    BSP_UART_Printf("  Protocol: FireWater\r\n");
    BSP_UART_Printf("=============================\r\n");
#endif
}

/*=========================== 主循环 ===========================*/

void Test_Phase0_Loop(void)
{
    uint32_t now = HAL_GetTick();

    /*---------- LED0 闪烁: 证明主循环活着 ----------*/
    if (now - tick_led >= TEST_LED_INTERVAL_MS) {
        tick_led = now;
        LED_Toggle(LED0);
    }

    /*---------- 按键扫描 + 事件打印 (FireWater模式才打印文本) ----------*/
    if (now - tick_key >= TEST_KEY_INTERVAL_MS) {
        tick_key = now;
        Key_Scan(&hkey);

#if (TEST_VOFA_PROTOCOL != 0)
        for (uint8_t i = 0; i < KEY_NUM; i++) {
            KeyEvent_TypeDef evt = Key_GetEvent(&hkey, i);
            if (evt == KEY_EVENT_SHORT_PRESS) {
                BSP_UART_Printf("[KEY%d] Short Press\r\n", i);
            } else if (evt == KEY_EVENT_LONG_PRESS) {
                BSP_UART_Printf("[KEY%d] Long Press\r\n", i);
            } else if (evt == KEY_EVENT_LONG_RELEASE) {
                BSP_UART_Printf("[KEY%d] Long Release\r\n", i);
            }
        }
#endif
    }

    /*---------- Vofa 正弦波测试数据 ----------*/
    if (now - tick_vofa >= TEST_VOFA_INTERVAL_MS) {
        tick_vofa = now;

        /* 生成 3 通道测试信号:
         *   CH0: sin(angle)
         *   CH1: cos(angle) 
         *   CH2: 三角波 (模拟占空比变化)
         */
        float data[3];
        data[0] = sinf(angle);
        data[1] = cosf(angle);
        data[2] = (angle < 3.14159f) ? (angle / 3.14159f) : (2.0f - angle / 3.14159f);

        angle += 0.1f;
        if (angle >= 6.28318f) angle = 0.0f;

#if (TEST_VOFA_PROTOCOL == 0)
        /* ★ JustFloat: Vofa上位机必须选 "JustFloat" 协议 ★ */
        VOFA_JustFloat(data, 3);
#else
        /* ★ FireWater: Vofa上位机必须选 "FireWater" 协议 ★ */
        VOFA_FireWaterFloat(data, 3);
#endif
    }
}
