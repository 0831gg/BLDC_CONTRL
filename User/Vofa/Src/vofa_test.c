#include "vofa_uart.h"
#include "vofa_port_config.h"
#include <math.h>
#include <string.h>

#if VOFA_ENABLE_TEST

/*=============================================================================
 *  VOFA+ 完整验证测试
 *
 *  包含 5 个递进测试用例:
 *    Test1: 波特率降级逻辑验证（纯计算，不需要上位机）
 *    Test2: 文本回显测试（FireWater协议）
 *    Test3: 正弦波形测试（JustFloat协议）
 *    Test4: 多通道模拟电机数据（JustFloat协议）
 *    Test5: 混合协议压力测试
 *
 *  验证方法见每个测试函数上方的注释
 *=============================================================================*/

/* 简易延时 */
static void delay_ms(uint32_t ms)
{
#if VOFA_USE_HAL
    HAL_Delay(ms);
#else
    volatile uint32_t i;
    /* 粗略延时，168MHz主频下约1ms */
    for (; ms > 0; ms--) {
        for (i = 0; i < 42000; i++);
    }
#endif
}

/*=============================================================================
 *  Test 1: 波特率降级逻辑验证
 *
 *  验证方法:
 *    1. 先用 115200 波特率连接串口终端（任意串口工具）
 *    2. 运行此测试，观察打印的波特率误差表
 *    3. 确认选择的波特率是误差 <= 2% 的最高波特率
 *
 *  预期输出（F407, APB2=84MHz）:
 *    4000000 OVER8  → err=0.00%  ← 应该选这个
 *    4000000 OVER16 → err=XX.XX% (超标)
 *    2000000 OVER8  → err=0.00%
 *    ...
 *=============================================================================*/
void VOFA_Test1_BaudScan(void)
{
    static const uint32_t test_bauds[] = VOFA_BAUD_TABLE;
    uint8_t i;
    float err8, err16;
    const VOFA_Info_t* info;

    VOFA_Init();
    info = VOFA_GetInfo();

    VOFA_Printf("\r\n===== Test1: Baud Rate Scan =====\r\n");
    VOFA_Printf("PCLK = %lu Hz\r\n", (unsigned long)VOFA_USART_PCLK_HZ);
    VOFA_Printf("Max Allowed Error = %.1f%%\r\n", VOFA_MAX_BAUD_ERROR);
    VOFA_Printf("---------------------------------\r\n");
    VOFA_Printf("%-10s %-10s %-10s %s\r\n", "Baudrate", "OVER8_Err", "OVER16_Err", "Status");
    VOFA_Printf("---------------------------------\r\n");

    for (i = 0; i < VOFA_BAUD_TABLE_SIZE; i++) {
        err8  = VOFA_Port_CalcBaudError(test_bauds[i], 1);
        err16 = VOFA_Port_CalcBaudError(test_bauds[i], 0);

        VOFA_Printf("%-10lu %-10.2f %-10.2f ",
                     (unsigned long)test_bauds[i], err8, err16);

        if (test_bauds[i] == info->baudrate) {
            VOFA_Printf("<-- SELECTED (%s)\r\n",
                         info->over8 ? "OVER8" : "OVER16");
        } else if (err8 <= VOFA_MAX_BAUD_ERROR || err16 <= VOFA_MAX_BAUD_ERROR) {
            VOFA_Printf("OK\r\n");
        } else {
            VOFA_Printf("SKIP (error too high)\r\n");
        }
    }

    VOFA_Printf("---------------------------------\r\n");
    VOFA_Printf("Final: %lu bps, %s, err=%.2f%%\r\n\r\n",
                (unsigned long)info->baudrate,
                info->over8 ? "OVER8" : "OVER16",
                info->baud_error);
}

/*=============================================================================
 *  Test 2: FireWater 文本回显
 *
 *  验证方法:
 *    1. VOFA+ 选择 "FireWater" 协议
 *    2. 波特率设为 VOFA_GetBaudrate() 返回的值
 *    3. 运行后应看到递增计数器文本
 *
 *  预期输出:
 *    [VOFA] Counter: 0
 *    [VOFA] Counter: 1
 *    [VOFA] Counter: 2
 *    ...
 *=============================================================================*/
void VOFA_Test2_FireWaterText(void)
{
    uint32_t cnt = 0;

    VOFA_Init();
    VOFA_Printf("\r\n===== Test2: FireWater Text =====\r\n");
    VOFA_Printf("Baudrate: %lu\r\n", (unsigned long)VOFA_GetBaudrate());

    while (1) {
        VOFA_Printf("[VOFA] Counter: %lu\r\n", (unsigned long)cnt++);
        delay_ms(500);
    }
}

/*=============================================================================
 *  Test 3: JustFloat 正弦波形
 *
 *  验证方法:
 *    1. VOFA+ 选择 "JustFloat" 协议
 *    2. 波特率设为程序启动时打印的值
 *    3. 应看到 3 条正弦波形:
 *       CH1: sin(t)         周期 ~6.28s
 *       CH2: sin(2t)        周期 ~3.14s
 *       CH3: sin(t) + 0.5   带偏移
 *
 *  预期效果:
 *    - 波形平滑无毛刺
 *    - 3条曲线频率和幅值正确
 *    - 如果波形有锯齿/乱码 → 波特率不匹配
 *=============================================================================*/
void VOFA_Test3_JustFloatSine(void)
{
    float ch[3];
    float t = 0.0f;
    uint16_t i;

    VOFA_Init();

    /* 等待Init文本发送完毕，再发同步帧帮助VOFA+对齐JustFloat帧边界 */
    delay_ms(100);
    memset(ch, 0, sizeof(ch));
    for (i = 0; i < 50; i++) {
        VOFA_JustFloat(ch, 3);
        delay_ms(2);
    }

    while (1) {
        ch[0] = sinf(t);
        ch[1] = sinf(2.0f * t);
        ch[2] = sinf(t) + 0.5f;

        VOFA_JustFloat(ch, 3);

        t += 0.02f;
        if (t > 6.2832f) t -= 6.2832f;

        delay_ms(2);
    }
}

/*=============================================================================
 *  Test 4: 模拟电机数据（4通道）
 *
 *  验证方法:
 *    1. VOFA+ 选择 "JustFloat" 协议
 *    2. 应看到 4 条曲线模拟 BLDC 运行:
 *       CH1: Ia 相电流  (正弦 ±10A)
 *       CH2: Speed 转速 (正弦 1000~2000 RPM)
 *       CH3: Vbus 母线电压 (24V + 纹波)
 *       CH4: Temp 温度  (缓慢上升 40~50°C)
 *=============================================================================*/
void VOFA_Test4_MotorSimulation(void)
{
    float motor[4];
    float t = 0.0f;
    uint16_t i;

    VOFA_Init();

    /* 等待Init文本发送完毕，再发同步帧帮助VOFA+对齐JustFloat帧边界 */
    delay_ms(100);
    memset(motor, 0, sizeof(motor));
    for (i = 0; i < 50; i++) {
        VOFA_JustFloat(motor, 4);
        delay_ms(2);
    }

    while (1) {
        /* CH1: 相电流 */
        motor[0] = 10.0f * sinf(t * 50.0f);
        /* CH2: 转速 */
        motor[1] = 1500.0f + 500.0f * sinf(t * 0.5f);
        /* CH3: 母线电压 + 纹波 */
        motor[2] = 24.0f + 0.3f * sinf(t * 100.0f);
        /* CH4: 温度缓慢变化 */
        motor[3] = 45.0f + 5.0f * sinf(t * 0.05f);

        VOFA_JustFloat(motor, 4);

        t += 0.001f;
        delay_ms(2);
    }
}

/*=============================================================================
 *  Test 5: 混合协议压力测试
 *
 *  验证方法:
 *    1. 先用 "FireWater" 协议观察文本 + 波形数据
 *    2. 切换 "JustFloat" 观察波形
 *    3. 交替测试，确认两种协议均正常
 *    4. 长时间运行（>30分钟）检查是否有数据丢失/错位
 *
 *  此测试在 FireWater 和 JustFloat 间交替发送
 *=============================================================================*/
void VOFA_Test5_StressTest(void)
{
    float data[2];
    float t = 0.0f;
    uint32_t frame = 0;

    VOFA_Init();
    VOFA_Printf("[VOFA] Stress Test Start\r\n");

    while (1) {
        data[0] = sinf(t);
        data[1] = cosf(t);

        if (frame % 2 == 0) {
            /* 偶数帧: JustFloat */
            VOFA_JustFloat(data, 2);
        } else {
            /* 奇数帧: FireWater */
            VOFA_FireWaterFloat(data, 2);
        }

        t += 0.05f;
        if (t > 6.2832f) t -= 6.2832f;
        frame++;

        /* 每10000帧报告一次 */
        if (frame % 10000 == 0) {
            VOFA_Printf("[VOFA] Frames: %lu\r\n", (unsigned long)frame);
        }

        delay_ms(1);
    }
}

/*=============================================================================
 *  统一测试入口
 *=============================================================================*/

/**
 * @brief  运行指定测试
 * @param  test_id: 测试编号 1~5
 */
void VOFA_RunTest(uint8_t test_id)
{
    switch (test_id) {
        case 1: VOFA_Test1_BaudScan();        break;
        case 2: VOFA_Test2_FireWaterText();   break;
        case 3: VOFA_Test3_JustFloatSine();   break;
        case 4: VOFA_Test4_MotorSimulation(); break;
        case 5: VOFA_Test5_StressTest();      break;
        default:
            VOFA_Init();
            VOFA_Printf("[VOFA] Invalid test_id: %d (valid: 1~5)\r\n", test_id);
            break;
    }
}

#endif /* VOFA_ENABLE_TEST */
