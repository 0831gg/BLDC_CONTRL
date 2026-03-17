/**
 * @file    bsp_hall.c
 * @brief   霍尔传感器驱动实现
 * @details 实现霍尔传感器的状态读取、方向检测、转速计算等功能
 * @see     bsp_hall.h
 */

#include "bsp_hall.h"

#include "tim.h"
#include "motor_ctrl.h"

/* 外部函数声明 - motor_sensor_mode_phase 未在 motor_ctrl.h 中声明 */
extern void motor_sensor_mode_phase(void);

/*============================================================================
 * 私有常量定义
 *============================================================================*/

/** @brief TIM5定时器时钟频率: 170MHz / (1679+1) = 101190Hz */
#define BSP_HALL_TIM5_TICK_HZ              (101190UL)
/** @brief 电机极对数 */
#define BSP_HALL_POLE_PAIRS                (4UL)
/** @brief 每转电气换相次数 = 6步 × 极对数 */
#define BSP_HALL_TRANSITIONS_PER_REV       (6UL * BSP_HALL_POLE_PAIRS)
/** @brief TIM5启动失败错误码 */
#define BSP_HALL_ERROR_TIM5_START          (1U)
/** @brief RPM滑动平均滤波窗口大小 */
#define BSP_HALL_SPEED_FILTER_SIZE         (6U)

/*============================================================================
 * 私有变量定义
 *============================================================================*/

/** @brief 当前霍尔状态 */
static volatile uint8_t s_hall_state = 0U;
/** @brief 上一次有效的霍尔状态 */
static volatile uint8_t s_hall_last_state = 0U;
/** @brief 最近一次换相的定时器计数值 */
static volatile uint32_t s_last_commutation_time = 0U;
/** @brief 最近一次换相的周期（定时器滴答） */
static volatile uint32_t s_last_period_ticks = 0U;
/** @brief 边沿跳变总计数 */
static volatile uint32_t s_edge_count = 0U;
/** @brief 有效换相计数 */
static volatile uint32_t s_valid_transition_count = 0U;
/** @brief 无效换相计数 */
static volatile uint32_t s_invalid_transition_count = 0U;
/** @brief 当前旋转方向 */
static volatile int8_t s_direction = BSP_HALL_DIR_UNKNOWN;
/** @brief EXTI中断使能标志（保留用于兼容） */
static volatile uint8_t s_irq_enabled = 0U;

/** @brief RPM滤波缓冲区 - 存储最近N次换相周期 */
static volatile uint32_t s_period_buffer[BSP_HALL_SPEED_FILTER_SIZE] = {0};
/** @brief RPM滤波缓冲区写入索引 */
static volatile uint8_t s_period_buffer_index = 0U;
/** @brief RPM滤波缓冲区有效数据计数 */
static volatile uint8_t s_period_buffer_count = 0U;

/*============================================================================
 * 私有函数实现
 *============================================================================*/

/**
 * @brief 采样当前霍尔传感器状态
 * @return uint8_t 霍尔状态编码 (W:V:U格式)
 * @note 直接读取GPIO引脚状态，不使用EXTI中断
 */
static uint8_t bsp_hall_sample_state(void)
{
    uint8_t state = 0U;

    /* 读取霍尔U相状态 (HALL_U引脚) */
    if (HAL_GPIO_ReadPin(HALL_U_GPIO_Port, HALL_U_Pin) == GPIO_PIN_SET) {
        state |= 0x01U;
    }

    /* 读取霍尔V相状态 (HALL_V引脚) */
    if (HAL_GPIO_ReadPin(HALL_V_GPIO_Port, HALL_V_Pin) == GPIO_PIN_SET) {
        state |= 0x02U;
    }

    /* 读取霍尔W相状态 (HALL_W引脚) */
    if (HAL_GPIO_ReadPin(HALL_W_GPIO_Port, HALL_W_Pin) == GPIO_PIN_SET) {
        state |= 0x04U;
    }

    return state;
}

/**
 * @brief 检查霍尔状态是否有效
 * @param state 霍尔状态编码
 * @return uint8_t 1:有效, 0:无效
 * @note 有效状态为0x01,0x02,0x03,0x04,0x05,0x06
 */
static uint8_t bsp_hall_is_valid_state(uint8_t state)
{
    return (uint8_t)((state == 0x01U) ||
                     (state == 0x05U) ||
                     (state == 0x04U) ||
                     (state == 0x06U) ||
                     (state == 0x02U) ||
                     (state == 0x03U));
}

/**
 * @brief 获取顺时针方向的下一个有效状态
 * @param state 当前状态
 * @return uint8_t 下一个状态
 * @note CW方向: 0x06→0x04→0x05→0x01→0x03→0x02→0x06
 */
static uint8_t bsp_hall_get_next_state(uint8_t state)
{
    switch (state) {
        case 0x06U: return 0x04U;
        case 0x04U: return 0x05U;
        case 0x05U: return 0x01U;
        case 0x01U: return 0x03U;
        case 0x03U: return 0x02U;
        case 0x02U: return 0x06U;
        default:    return 0x00U;
    }
}

/**
 * @brief 获取逆时针方向的下一个有效状态
 * @param state 当前状态
 * @return uint8_t 下一个状态
 * @note CCW方向: 0x06→0x02→0x03→0x01→0x05→0x04→0x06
 */
static uint8_t bsp_hall_get_prev_state(uint8_t state)
{
    switch (state) {
        case 0x06U: return 0x02U;
        case 0x02U: return 0x03U;
        case 0x03U: return 0x01U;
        case 0x01U: return 0x05U;
        case 0x05U: return 0x04U;
        case 0x04U: return 0x06U;
        default:    return 0x00U;
    }
}

/*============================================================================
 * 公共函数实现
 *============================================================================*/

/**
 * @brief 初始化霍尔传感器模块
 * @return uint8_t 0:成功, BSP_HALL_ERROR_TIM5_START:TIM5启动失败
 * @note 启动TIM5定时器，采样初始霍尔状态用于后续换相判断
 */
uint8_t bsp_hall_init(void)
{
    uint32_t now;
    uint8_t state;

    /* 启动TIM5定时器（用于精确计时） */
    if (HAL_TIM_Base_Start(&htim5) != HAL_OK) {
        return BSP_HALL_ERROR_TIM5_START;
    }

    /* 采样初始霍尔状态 */
    state = bsp_hall_sample_state();
    now = __HAL_TIM_GET_COUNTER(&htim5);

    /* 初始化所有状态变量 */
    s_hall_state = state;
    s_hall_last_state = state;
    s_last_commutation_time = now;
    s_last_period_ticks = 0U;
    s_edge_count = 0U;
    s_valid_transition_count = 0U;
    s_invalid_transition_count = 0U;
    s_direction = BSP_HALL_DIR_UNKNOWN;

    /* 注意：不再使用EXTI中断，改用TIM1更新中断轮询 */
    return 0U;
}

/**
 * @brief 获取当前霍尔状态
 * @return uint8_t 当前霍尔状态编码
 */
uint8_t bsp_hall_get_state(void)
{
    return s_hall_state;
}

/**
 * @brief 获取上一次有效的霍尔状态
 * @return uint8_t 上一次状态编码
 */
uint8_t bsp_hall_get_last_state(void)
{
    return s_hall_last_state;
}

/**
 * @brief 计算电机转速（带滑动平均滤波）
 * @return uint32_t 转速(RPM)
 * @note 公式: RPM = (TIM5频率 × 60) / (平均周期滴答数 × 6)
 *       使用最近6次换相周期的平均值，减少瞬时波动
 */
uint32_t bsp_hall_get_speed(void)
{
    uint32_t sum = 0U;
    uint32_t avg_ticks;
    uint8_t i;
    uint8_t count;
    uint32_t buf_snap[BSP_HALL_SPEED_FILTER_SIZE];

    /* [Fix5] 关中断，原子拷贝缓冲区快照，防止读写竞态 */
    NVIC_DisableIRQ(TIM1_UP_TIM16_IRQn);
    count = s_period_buffer_count;
    for (i = 0U; i < count; i++) {
        buf_snap[i] = s_period_buffer[i];
    }
    NVIC_EnableIRQ(TIM1_UP_TIM16_IRQn);

    /* 没有有效数据时返回0 */
    if (count == 0U) {
        return 0U;
    }

    /* 计算快照中所有有效周期的总和 */
    for (i = 0U; i < count; i++) {
        sum += buf_snap[i];
    }

    /* 计算平均周期 */
    avg_ticks = sum / count;

    /* 防止除零 */
    if (avg_ticks == 0U) {
        return 0U;
    }

    /* 基于平均周期计算RPM */
    return (uint32_t)((BSP_HALL_TIM5_TICK_HZ * 60UL) / (avg_ticks * BSP_HALL_TRANSITIONS_PER_REV));
}

/**
 * @brief 获取有符号转速
 * @return int32_t CW为正, CCW为负, 未知方向返回0
 */
int32_t bsp_hall_get_speed_signed(void)
{
    uint32_t rpm = bsp_hall_get_speed();
    int8_t dir = s_direction;

    if (dir == BSP_HALL_DIR_CW) {
        return (int32_t)rpm;
    } else if (dir == BSP_HALL_DIR_CCW) {
        return -(int32_t)rpm;
    }
    return 0;
}

/**
 * @brief 获取最近一次换相的时刻
 * @return uint32_t TIM5定时器计数值
 */
uint32_t bsp_hall_get_commutation_time(void)
{
    return s_last_commutation_time;
}

/**
 * @brief 获取最近一次换相的周期（滴答数）
 * @return uint32_t 周期滴答数
 */
uint32_t bsp_hall_get_period_ticks(void)
{
    return s_last_period_ticks;
}

/**
 * @brief 获取最近一次换相的周期（微秒）
 * @return uint32_t 周期微秒数
 */
uint32_t bsp_hall_get_period_us(void)
{
    return (uint32_t)((s_last_period_ticks * 1000000ULL) / BSP_HALL_TIM5_TICK_HZ);
}

/**
 * @brief 获取边沿跳变总计数
 * @return uint32_t 计数
 */
uint32_t bsp_hall_get_edge_count(void)
{
    return s_edge_count;
}

/**
 * @brief 获取有效换相次数
 * @return uint32_t 有效转换计数
 */
uint32_t bsp_hall_get_valid_transition_count(void)
{
    return s_valid_transition_count;
}

/**
 * @brief 获取无效换相次数
 * @return uint32_t 无效转换计数
 */
uint32_t bsp_hall_get_invalid_transition_count(void)
{
    return s_invalid_transition_count;
}

/**
 * @brief 获取当前旋转方向
 * @return int8_t 方向: 1=CW, -1=CCW, 0=未知
 */
int8_t bsp_hall_get_direction(void)
{
    return s_direction;
}

/**
 * @brief 检查中断是否使能
 * @return uint8_t 1:使能, 0:禁用
 */
uint8_t bsp_hall_is_irq_enabled(void)
{
    return s_irq_enabled;
}

/**
 * @brief 清除所有统计和状态数据
 * @note 保留当前GPIO采样的状态作为起始点，重新开始计时
 */
void bsp_hall_clear_stats(void)
{
    uint32_t now;
    uint8_t state;
    uint8_t i;

    /* [Fix7] 关中断，防止清零序列被TIM1中断打断导致状态不一致 */
    NVIC_DisableIRQ(TIM1_UP_TIM16_IRQn);

    now = __HAL_TIM_GET_COUNTER(&htim5);
    state = bsp_hall_sample_state();

    s_hall_state = state;
    s_hall_last_state = state;
    s_last_commutation_time = now;
    s_last_period_ticks = 0U;
    s_edge_count = 0U;
    s_valid_transition_count = 0U;
    s_invalid_transition_count = 0U;
    s_direction = BSP_HALL_DIR_UNKNOWN;

    for (i = 0U; i < BSP_HALL_SPEED_FILTER_SIZE; i++) {
        s_period_buffer[i] = 0U;
    }
    s_period_buffer_index = 0U;
    s_period_buffer_count = 0U;

    NVIC_EnableIRQ(TIM1_UP_TIM16_IRQn);
}

/**
 * @brief 使能霍尔传感器中断（保留接口，禁用功能）
 * @note 当前架构已改为TIM1轮询，此函数仅更新标志位
 */
void bsp_hall_irq_enable(void)
{
    /* EXTI中断已禁用，改用定时器轮询 */
    s_irq_enabled = 1U;
}

/**
 * @brief 禁用霍尔传感器中断（保留接口，禁用功能）
 * @note 当前架构已改为TIM1轮询，此函数仅更新标志位
 */
void bsp_hall_irq_disable(void)
{
    /* EXTI中断已禁用，改用定时器轮询 */
    s_irq_enabled = 0U;
}

/**
 * @brief 轮询霍尔状态并执行换相
 * @note 在TIM1更新中断中调用，检测霍尔状态变化并触发换相
 *       1. 采样新霍尔状态
 *       2. 判断是否为有效变化
 *       3. 计算方向和转速
 *       4. 调用motor_sensor_mode_phase()执行换相
 */
void bsp_hall_poll_and_commutate(void)
{
    uint8_t new_state;
    uint8_t old_state;
    uint32_t now;

    /* 采样新的霍尔状态 */
    new_state = bsp_hall_sample_state();
    old_state = s_hall_state;

    /* 状态未变化，无需处理 */
    if (new_state == old_state) {
        return;
    }

    /* 状态变化，处理换相逻辑 */
    s_edge_count++;
    s_hall_last_state = old_state;
    s_hall_state = new_state;

    /* 检查是否为有效的状态转换 */
    if (bsp_hall_is_valid_state(old_state) && bsp_hall_is_valid_state(new_state)) {
        /* 判断旋转方向 */
        if (bsp_hall_get_next_state(old_state) == new_state) {
            /* 逆时针旋转 */
            s_direction = BSP_HALL_DIR_CCW;
        } else if (bsp_hall_get_prev_state(old_state) == new_state) {
            /* 顺时针旋转 */
            s_direction = BSP_HALL_DIR_CW;
        } else {
            /* 无效转换（跳跃超过一个状态） */
            s_direction = BSP_HALL_DIR_UNKNOWN;
            s_invalid_transition_count++;
            return;
        }

        /* 计算换相周期 */
        now = __HAL_TIM_GET_COUNTER(&htim5);
        s_last_period_ticks = now - s_last_commutation_time;
        s_last_commutation_time = now;
        s_valid_transition_count++;

        /* 更新RPM滤波缓冲区 */
        s_period_buffer[s_period_buffer_index] = s_last_period_ticks;
        s_period_buffer_index = (uint8_t)((s_period_buffer_index + 1U) % BSP_HALL_SPEED_FILTER_SIZE);
        if (s_period_buffer_count < BSP_HALL_SPEED_FILTER_SIZE) {
            s_period_buffer_count++;
        }

        /* 执行电机换相 */
        motor_sensor_mode_phase();
    } else {
        /* 包含无效状态的转换 */
        s_direction = BSP_HALL_DIR_UNKNOWN;
        s_invalid_transition_count++;
    }
}
