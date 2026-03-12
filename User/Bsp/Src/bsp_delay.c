#include "bsp_delay.h"
#include "stm32g4xx_hal.h"

static uint8_t dwt_enable_flag = 0;

/**
 * @brief  检查 DWT 单元是否可用
 * @note   DWT 需要调试器连接或处于非特权模式才能正常工作
 *          如果在特权模式下运行且无调试器,此函数将返回 0
 * @retval 0: DWT 不可用; 1: DWT 可用
 */
static uint8_t Delay_CheckDWT(void)
{
    uint32_t ctrl;
    
    CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
    
    // 2. 读取并验证 DWT 控制寄存器
    ctrl = DWT->CTRL;
    
    // 3. 检查 CYCCNTENA 位是否可设置
    DWT->CTRL = ctrl | DWT_CTRL_CYCCNTENA_Msk;
    
    // 4. 验证设置是否生效
    if ((DWT->CTRL & DWT_CTRL_CYCCNTENA_Msk) == 0)
    {
        return 0; // DWT 不可用
    }
    
    // 5. 清空计数器并启动
    DWT->CYCCNT = 0;
    
    // 6. 写入一次验证计数器是否工作
    DWT->CYCCNT = 0;
    __NOP();
    __NOP();
    __NOP();
    if (DWT->CYCCNT == 0)
    {
        return 0; // 计数器不工作
    }
    
    return 1; // DWT 可用
}


/** @brief  初始化 DWT 定时器
  * @note   必须在使用延时函数前调用
  *          如果返回 0,说明 DWT 不可用,延时函数将不工作
  *          可能原因: 处于特权模式运行、无调试器连接、Cortex-M 核心不支持
  * @param  None                                                                                                                                                       
  * @retval 0: 初始化失败 DWT不可用; 1: 初始化成功
  */
uint32_t Delay_Init(void)
{
    dwt_enable_flag = Delay_CheckDWT();
    return dwt_enable_flag;
}



/** @brief  微秒级延时函数
  * @note   如果 DWT 未初始化或初始化失败,此函数将直接返回不延时
  * @param  us: 延时微秒数
  * @retval None
  */
void Delay_us(uint32_t us)
{
    if (dwt_enable_flag == 0) return;
    
    uint32_t ticks = us * (SystemCoreClock / 1000000); 
    uint32_t t0 = DWT->CYCCNT; 
    
    while ((DWT->CYCCNT - t0) < ticks) 
    {
        __NOP();
    }
}

/** @brief  毫秒级延时函数
  * @note   如果 DWT 未初始化或初始化失败,此函数将直接返回不延时
  * @param  ms: 延时毫秒数
  * @retval None
  */
void Delay_ms(uint32_t ms)
{
    if (dwt_enable_flag == 0) return;
    
    for (uint32_t i = 0; i < ms; i++)
    {
        Delay_us(1000);
    }
}


/** @brief  微秒级延时测试函数
  * @note   如果 DWT 未初始化或初始化失败,此函数将直接返回不执行
  * @param  times: 测试次数
  * @retval None
  */ 
void Delay_us_test(uint8_t times)
{
    if (dwt_enable_flag == 0) return;
    
    for (uint8_t i = 0; i < times; i++)
    {
        HAL_GPIO_WritePin(GPIOA, GPIO_PIN_5, GPIO_PIN_SET);
        Delay_us(500);
        HAL_GPIO_WritePin(GPIOA, GPIO_PIN_5, GPIO_PIN_RESET);
        Delay_us(500);
    }
}
