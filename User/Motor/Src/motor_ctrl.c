/**
 * @file    motor_ctrl.c
 * @brief   BLDC电机控制实现
 * @details 实现电机的启动、停止、换相控制等功能
 *          基于霍尔传感器的六步换向算法
 * @see     motor_ctrl.h
 */

#include "motor_ctrl.h"

#include "bsp_ctrl_sd.h"
#include "bsp_delay.h"
#include "bsp_hall.h"
#include "bsp_pwm.h"
#include "pid.h"

#include <math.h>

/*============================================================================
 * 私有常量定义
 *============================================================================*/

/** @brief 启动准备延时 (ms) */
#define MOTOR_START_PREPARE_DELAY_MS    (2U)//
/** @brief 预定位延时 (ms) */
#define MOTOR_START_ALIGN_DELAY_MS      (80U)
/** @brief 辅助启动超时 (ms) */
#define MOTOR_START_ASSIST_TIMEOUT_MS   (300U)

/** @brief 辅助启动占空比 - 步骤1 (5%) */
#define MOTOR_START_ASSIST_DUTY_STEP1   (((BSP_PWM_PERIOD + 1U) * 5U) / 100U)
/** @brief 辅助启动占空比 - 步骤2 (8%) */
#define MOTOR_START_ASSIST_DUTY_STEP2   (((BSP_PWM_PERIOD + 1U) * 8U) / 100U)
/** @brief 辅助启动占空比 - 步骤3 (10%) */
#define MOTOR_START_ASSIST_DUTY_STEP3   (((BSP_PWM_PERIOD + 1U) * 10U) / 100U)

/** @brief 辅助启动各步骤延时 (ms) */
static const uint8_t s_start_assist_step_delay_ms[] = {20U, 16U, 14U, 12U, 10U, 8U};

/*============================================================================
 * 类型定义
 *============================================================================*/

/**
 * @brief 启动阶段枚举
 */
typedef enum {
    MOTOR_STARTUP_STAGE_IDLE = 0,      /**< @brief 空闲 */
    MOTOR_STARTUP_STAGE_PREPARE,        /**< @brief 准备 */
    MOTOR_STARTUP_STAGE_ALIGN,          /**< @brief 预定位 */
    MOTOR_STARTUP_STAGE_KICK,           /**< @brief 强推动 */
    MOTOR_STARTUP_STAGE_WAIT_EDGE,      /**< @brief 等待边沿 */
    MOTOR_STARTUP_STAGE_TRACKING        /**< @brief 正常运行 */
} motor_startup_stage_t;

/*============================================================================
 * 私有变量定义
 *============================================================================*/

/** @brief 电机运行标志 */
static uint8_t s_motor_running = 0U;
/** @brief 电机方向 */
static uint8_t s_motor_direction = MOTOR_DIR_CCW;
/** @brief 故障标志 */
static uint8_t s_motor_fault = MOTOR_FAULT_NONE;
/** @brief 当前占空比 */
static uint16_t s_motor_duty = 0U;
/** @brief 上一次霍尔状态 */
static uint8_t s_last_hall_state = 0U;
/** @brief 上一次换相动作 */
static motor_phase_action_t s_last_action = MOTOR_PHASE_ACTION_INVALID;
/** @brief 当前启动阶段 */
static uint8_t s_startup_stage = MOTOR_STARTUP_STAGE_IDLE;
/** @brief 启动步骤号 */
static uint8_t s_startup_step = 0U;
/** @brief 启动时霍尔状态 */
static uint8_t s_startup_hall_state = 0U;
/** @brief 启动时换相动作 */
static motor_phase_action_t s_startup_action = MOTOR_PHASE_ACTION_INVALID;
/** @brief 启动时占空比 */
static uint16_t s_startup_duty = 0U;

/** @brief 速度环PID实例 */
static speed_pid_t s_speed_pid;
/** @brief 控制模式 */
static uint8_t s_control_mode = MOTOR_MODE_OPEN_LOOP;
/** @brief 目标转速 (有符号RPM) */
static int32_t s_speed_target = 0;
/** @brief PID输出滤波值 */
static float s_pid_output_filtered = 0.0f;

/** @brief PID低通滤波系数 */
#define PID_LPF_ALPHA  (0.085f)
/** @brief PID启动最小占空比 */
#define PID_MIN_START_DUTY  (((BSP_PWM_PERIOD + 1U) * 5U) / 100U)

/*============================================================================
 * 私有函数实现
 *============================================================================*/

/**
 * @brief 设置启动跟踪信息
 * @note 用于调试和状态显示
 */
static void motor_ctrl_set_startup_trace(uint8_t stage,
                                         uint8_t step,
                                         uint8_t hall_state,
                                         motor_phase_action_t action,
                                         uint16_t duty)
{
    s_startup_stage = stage;
    s_startup_step = step;
    s_startup_hall_state = hall_state;
    s_startup_action = action;
    s_startup_duty = duty;
}

/**
 * @brief 准备启动
 * @param duty 占空比
 * @param direction 方向
 * @note 清除统计、禁用驱动、设置PWM
 */
static void motor_ctrl_prepare_start(uint16_t duty, uint8_t direction)
{
    s_motor_fault = MOTOR_FAULT_NONE;
    s_motor_direction = direction;
    s_motor_duty = duty;

    bsp_hall_clear_stats();
    bsp_ctrl_sd_disable();
    bsp_pwm_clear_break_fault();
    bsp_pwm_lower_all_off();
    /* 阶段2：删除bsp_pwm_all_close()和bsp_pwm_all_open()，PWM保持运行 */
    bsp_pwm_duty_set(s_motor_duty);
    s_motor_running = 1U;
    motor_ctrl_set_startup_trace(MOTOR_STARTUP_STAGE_PREPARE,
                                 0U,
                                 bsp_hall_get_state(),
                                 MOTOR_PHASE_ACTION_INVALID,
                                 s_motor_duty);
}

/**
 * @brief 获取辅助启动占空比
 * @param duty 请求的占空比
 * @param step 当前步骤
 * @return uint16_t 实际使用的占空比
 */
static uint16_t motor_ctrl_get_assist_duty(uint16_t duty, uint8_t step)
{
    (void)duty;

    if (step < 2U) {
        return MOTOR_START_ASSIST_DUTY_STEP1;
    } else if (step < 4U) {
        return MOTOR_START_ASSIST_DUTY_STEP2;
    } else {
        return MOTOR_START_ASSIST_DUTY_STEP3;
    }
}

/**
 * @brief 获取下一个霍尔状态（根据方向）
 * @param direction 方向
 * @param state 当前状态
 * @return uint8_t 下一状态
 */
static uint8_t motor_ctrl_get_next_hall_state(uint8_t direction, uint8_t state)
{
    if (direction == MOTOR_DIR_CCW) {
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

/**
 * @brief 应用霍尔换相动作
 * @param direction 方向
 * @param hall_state 霍尔状态
 * @param duty 占空比
 * @return 0:成功, -1:失败
 */
static int motor_ctrl_apply_hall_action(uint8_t direction, uint8_t hall_state, uint16_t duty)
{
    motor_phase_action_t action = motor_phase_tab_get(direction, hall_state);

    s_last_hall_state = hall_state;
    s_last_action = action;

    if (action == MOTOR_PHASE_ACTION_INVALID) {
        motor_ctrl_set_fault(MOTOR_FAULT_HALL_INVALID);
        bsp_pwm_lower_all_off();
        bsp_pwm_ctrl_single_phase(BSP_PWM_PHASE_NONE);
        return -1;
    }

    motor_phase_apply(action, duty);
    return 0;
}

/**
 * @brief 检查Break故障
 * @return 0:正常, 1:有故障
 */
static uint8_t motor_ctrl_check_break_fault(void)
{
    if (bsp_pwm_is_break_fault() == 0U) {
        return 0U;
    }

    bsp_pwm_clear_break_fault();
    motor_ctrl_set_fault(MOTOR_FAULT_BREAK);
    return 1U;
}

/*============================================================================
 * 公共函数实现
 *============================================================================*/

/**
 * @brief 初始化电机控制
 */
void motor_ctrl_init(void)
{
    motor_phase_tab_init_defaults();
    s_motor_running = 0U;
    s_motor_direction = MOTOR_DIR_CCW;
    s_motor_fault = MOTOR_FAULT_NONE;
    s_motor_duty = 0U;
    s_last_hall_state = 0U;
    s_last_action = MOTOR_PHASE_ACTION_INVALID;
    s_control_mode = MOTOR_MODE_OPEN_LOOP;
    s_speed_target = 0;
    s_pid_output_filtered = 0.0f;
    pid_init(&s_speed_pid, 0.008f, 0.00025f, 0.0002f);
    pid_set_limits(&s_speed_pid, -(float)BSP_PWM_PERIOD, (float)BSP_PWM_PERIOD);
    motor_ctrl_set_startup_trace(MOTOR_STARTUP_STAGE_IDLE,
                                 0U,
                                 0U,
                                 MOTOR_PHASE_ACTION_INVALID,
                                 0U);

    bsp_pwm_lower_all_off();
    bsp_pwm_all_close();
    bsp_ctrl_sd_disable();
}

/**
 * @brief 简单启动
 */
int motor_start(uint16_t duty, uint8_t direction)
{
    motor_ctrl_prepare_start(duty, direction);

    motor_sensor_mode_phase();
    Delay_ms(MOTOR_START_PREPARE_DELAY_MS);

    if ((s_motor_fault != MOTOR_FAULT_NONE) ||
        (motor_ctrl_check_break_fault() != 0U)) {
        motor_stop();
        return -1;
    }

    bsp_ctrl_sd_enable();
    return 0;
}

/**
 * @brief 辅助启动（预定位+强推动）
 */
int motor_start_assisted(uint16_t duty, uint8_t direction)
{
    uint16_t requested_duty = duty;
    uint16_t assist_duty;
    uint8_t assist_state;
    motor_phase_action_t assist_action;
    uint32_t start_tick;
    uint32_t i;

    /* 获取初始占空比并准备启动 */
    assist_duty = motor_ctrl_get_assist_duty(duty, 0U);
    motor_ctrl_prepare_start(assist_duty, direction);

    /* 预定位阶段 */
    assist_state = bsp_hall_get_state();
    assist_action = motor_phase_tab_get(direction, assist_state);
    motor_ctrl_set_startup_trace(MOTOR_STARTUP_STAGE_ALIGN,
                                 0U,
                                 assist_state,
                                 assist_action,
                                 assist_duty);
    if (motor_ctrl_apply_hall_action(direction, assist_state, assist_duty) != 0) {
        motor_stop();
        return -1;
    }

    bsp_ctrl_sd_enable();
    Delay_ms(MOTOR_START_ALIGN_DELAY_MS);

    if ((s_motor_fault != MOTOR_FAULT_NONE) ||
        (motor_ctrl_check_break_fault() != 0U)) {
        motor_stop();
        return -1;
    }

    /* 强推动阶段 - 按顺序切换霍尔状态 */
    for (i = 0U; i < (sizeof(s_start_assist_step_delay_ms) / sizeof(s_start_assist_step_delay_ms[0])); i++) {
        assist_duty = motor_ctrl_get_assist_duty(duty, (uint8_t)i);
        assist_state = motor_ctrl_get_next_hall_state(direction, assist_state);
        if (assist_state == 0U) {
            motor_ctrl_set_fault(MOTOR_FAULT_HALL_INVALID);
            motor_stop();
            return -1;
        }

        assist_action = motor_phase_tab_get(direction, assist_state);
        motor_ctrl_set_startup_trace(MOTOR_STARTUP_STAGE_KICK,
                                     (uint8_t)(i + 1U),
                                     assist_state,
                                     assist_action,
                                     assist_duty);
        if (motor_ctrl_apply_hall_action(direction, assist_state, assist_duty) != 0) {
            motor_stop();
            return -1;
        }

        Delay_ms(s_start_assist_step_delay_ms[i]);

        if ((s_motor_fault != MOTOR_FAULT_NONE) ||
            (motor_ctrl_check_break_fault() != 0U)) {
            motor_stop();
            return -1;
        }
    }

    /* 等待霍尔边沿 - 进入正常运行 */
    bsp_hall_clear_stats();
    motor_sensor_mode_phase();
    motor_ctrl_set_startup_trace(MOTOR_STARTUP_STAGE_WAIT_EDGE,
                                 0U,
                                 s_last_hall_state,
                                 s_last_action,
                                 assist_duty);
    if ((s_motor_fault != MOTOR_FAULT_NONE) ||
        (motor_ctrl_check_break_fault() != 0U)) {
        motor_stop();
        return -1;
    }

    start_tick = HAL_GetTick();

    /* 等待有效换相 */
    while ((HAL_GetTick() - start_tick) < MOTOR_START_ASSIST_TIMEOUT_MS) {
        if (bsp_hall_get_valid_transition_count() != 0U) {
            s_motor_duty = requested_duty;
            motor_sensor_mode_phase();
            motor_ctrl_set_startup_trace(MOTOR_STARTUP_STAGE_TRACKING,
                                         0U,
                                         s_last_hall_state,
                                         s_last_action,
                                         requested_duty);

            if ((s_motor_fault != MOTOR_FAULT_NONE) ||
                (motor_ctrl_check_break_fault() != 0U)) {
                motor_stop();
                return -1;
            }

            return 0;
        }

        Delay_ms(1U);
    }

    /* 启动超时 */
    motor_ctrl_set_fault(MOTOR_FAULT_START_TIMEOUT);
    motor_stop();
    return -1;
}

/**
 * @brief 简化启动（生产版本）
 */
int motor_start_simple(uint16_t duty, uint8_t direction)
{
    /* [Fix3] 启动前先禁用驱动和下桥，确保PWM状态干净 */
    bsp_ctrl_sd_disable();
    bsp_pwm_lower_all_off();

    s_motor_fault = MOTOR_FAULT_NONE;
    s_motor_direction = direction;

    /* [Fix6] duty上限边界检查 */
    if (duty > BSP_PWM_PERIOD) {
        duty = BSP_PWM_PERIOD;
    }
    s_motor_duty = duty;

    bsp_hall_clear_stats();
    bsp_pwm_clear_break_fault();
    bsp_pwm_duty_set(s_motor_duty);

    motor_ctrl_set_startup_trace(MOTOR_STARTUP_STAGE_TRACKING,
                                 0U,
                                 bsp_hall_get_state(),
                                 MOTOR_PHASE_ACTION_INVALID,
                                 s_motor_duty);

    /* 先置running=1，让motor_sensor_mode_phase()能够应用换相 */
    s_motor_running = 1U;
    motor_sensor_mode_phase();

    if (s_motor_fault != MOTOR_FAULT_NONE) {
        motor_stop();
        return -1;
    }

    /* 换相完成后再使能驱动 */
    bsp_ctrl_sd_enable();

    return 0;
}

/**
 * @brief 停止电机
 */
void motor_stop(void)
{
    bsp_ctrl_sd_disable();
    bsp_pwm_lower_all_off();
    bsp_pwm_all_close();
    s_motor_running = 0U;
    s_last_action = MOTOR_PHASE_ACTION_INVALID;
}

/**
 * @brief 传感器模式换相（霍尔触发）
 * @note 由bsp_hall_poll_and_commutate()调用
 */
void motor_sensor_mode_phase(void)
{
    motor_phase_action_t action;
    uint8_t hall_state = bsp_hall_get_state();

    s_last_hall_state = hall_state;
    action = motor_phase_tab_get(s_motor_direction, hall_state);
    s_last_action = action;

    if (s_motor_running == 0U) {
        return;
    }

    if (action == MOTOR_PHASE_ACTION_INVALID) {
        motor_ctrl_set_fault(MOTOR_FAULT_HALL_INVALID);
        bsp_pwm_lower_all_off();
        bsp_pwm_ctrl_single_phase(BSP_PWM_PHASE_NONE);
        return;
    }

    motor_phase_apply(action, s_motor_duty);
}

/**
 * @brief 设置故障
 */
void motor_ctrl_set_fault(uint8_t fault)
{
    s_motor_fault = fault;
}

/**
 * @brief 获取故障状态
 */
uint8_t motor_ctrl_get_fault(void)
{
    return s_motor_fault;
}

/**
 * @brief 检查是否运行
 */
uint8_t motor_ctrl_is_running(void)
{
    return s_motor_running;
}

/**
 * @brief 获取方向
 */
uint8_t motor_ctrl_get_direction(void)
{
    return s_motor_direction;
}

/**
 * @brief 设置方向
 */
void motor_ctrl_set_direction(uint8_t direction)
{
    s_motor_direction = direction;
}

/**
 * @brief 获取占空比
 */
uint16_t motor_ctrl_get_duty(void)
{
    return s_motor_duty;
}

/**
 * @brief 设置占空比
 */
void motor_ctrl_set_duty(uint16_t duty)
{
    s_motor_duty = duty;

    if (s_motor_running != 0U) {
        motor_sensor_mode_phase();
    }
}

/**
 * @brief 获取霍尔状态
 */
uint8_t motor_ctrl_get_hall_state(void)
{
    return s_last_hall_state;
}

/**
 * @brief 获取最后动作
 */
motor_phase_action_t motor_ctrl_get_last_action(void)
{
    return s_last_action;
}

/**
 * @brief 获取启动阶段文本
 */
const char *motor_ctrl_get_startup_stage_text(void)
{
    switch (s_startup_stage) {
        case MOTOR_STARTUP_STAGE_PREPARE:
            return "prepare";
        case MOTOR_STARTUP_STAGE_ALIGN:
            return "align";
        case MOTOR_STARTUP_STAGE_KICK:
            return "kick";
        case MOTOR_STARTUP_STAGE_WAIT_EDGE:
            return "wait_edge";
        case MOTOR_STARTUP_STAGE_TRACKING:
            return "tracking";
        case MOTOR_STARTUP_STAGE_IDLE:
        default:
            return "idle";
    }
}

/**
 * @brief 获取启动步骤
 */
uint8_t motor_ctrl_get_startup_step(void)
{
    return s_startup_step;
}

/**
 * @brief 获取启动霍尔状态
 */
uint8_t motor_ctrl_get_startup_hall_state(void)
{
    return s_startup_hall_state;
}

/**
 * @brief 获取启动动作
 */
motor_phase_action_t motor_ctrl_get_startup_action(void)
{
    return s_startup_action;
}

/**
 * @brief 获取启动占空比
 */
uint16_t motor_ctrl_get_startup_duty(void)
{
    return s_startup_duty;
}

/*============================================================================
 * 速度环控制
 *============================================================================*/

void motor_ctrl_set_mode(uint8_t mode)
{
    s_control_mode = mode;
    if (mode == MOTOR_MODE_SPEED_PID) {
        pid_reset(&s_speed_pid);
        s_pid_output_filtered = 0.0f;
    }
}

uint8_t motor_ctrl_get_mode(void)
{
    return s_control_mode;
}

void motor_ctrl_set_speed_target(int32_t rpm)
{
    s_speed_target = rpm;
    s_speed_pid.SetPoint = (float)rpm;
}

int32_t motor_ctrl_get_speed_target(void)
{
    return s_speed_target;
}

float motor_ctrl_get_pid_output(void)
{
    return s_pid_output_filtered;
}

speed_pid_t *motor_ctrl_get_pid(void)
{
    return &s_speed_pid;
}

void motor_ctrl_speed_pid_tick(void)
{
    float pid_out;
    float abs_out;
    uint16_t duty;
    uint8_t desired_dir;

    if (s_control_mode != MOTOR_MODE_SPEED_PID) {
        return;
    }
    if (s_motor_running == 0U) {
        return;
    }

    /* 读取有符号转速反馈 */
    float measured = (float)bsp_hall_get_speed_signed();

    /* PID 计算 */
    pid_out = pid_compute(&s_speed_pid, measured);

    /* 一阶低通滤波 */
    s_pid_output_filtered += PID_LPF_ALPHA * (pid_out - s_pid_output_filtered);

    /* 取绝对值作为占空比 */
    abs_out = (s_pid_output_filtered >= 0.0f) ? s_pid_output_filtered : -s_pid_output_filtered;
    duty = (uint16_t)abs_out;
    if (duty > BSP_PWM_PERIOD) {
        duty = BSP_PWM_PERIOD;
    }

    /* 方向由目标转速符号决定 */
    desired_dir = (s_speed_target >= 0) ? MOTOR_DIR_CW : MOTOR_DIR_CCW;

    if (desired_dir != s_motor_direction) {
        /* 方向变化：停机→换向→重启 */
        motor_stop();
        pid_reset(&s_speed_pid);
        s_pid_output_filtered = 0.0f;
        motor_start_simple(PID_MIN_START_DUTY, desired_dir);
        return;
    }

    /* 更新占空比并触发换相 */
    s_motor_duty = duty;
    motor_sensor_mode_phase();  /* 立即应用新占空比到当前霍尔状态 */
}
