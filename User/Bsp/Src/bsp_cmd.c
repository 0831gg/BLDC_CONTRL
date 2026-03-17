/**
 * @file    bsp_cmd.c
 * @brief   轻量ASCII串口命令解析实现
 * @details 支持命令: PID/SPD/RUN/STP/MOD/DTY
 */

#include "bsp_cmd.h"
#include "bsp_uart.h"
#include "bsp_pwm.h"
#include "motor_ctrl.h"
#include "usart.h"

#include <string.h>
#include <stdio.h>

#define CMD_BUF_SIZE  64U

static uint8_t s_rx_byte;
static char s_cmd_buf[CMD_BUF_SIZE];
static volatile uint8_t s_cmd_len = 0U;
static volatile uint8_t s_cmd_ready = 0U;

void bsp_cmd_rx_callback(uint8_t byte)
{
    if (s_cmd_ready) {
        return;  /* 上一条命令未处理完，丢弃 */
    }

    if (byte == '\n' || byte == '\r') {
        if (s_cmd_len > 0U) {
            s_cmd_buf[s_cmd_len] = '\0';
            s_cmd_ready = 1U;
        }
    } else {
        if (s_cmd_len < (CMD_BUF_SIZE - 1U)) {
            s_cmd_buf[s_cmd_len++] = (char)byte;
        }
    }
}

void bsp_cmd_init(void)
{
    s_cmd_len = 0U;
    s_cmd_ready = 0U;
    HAL_UART_Receive_IT(&huart1, &s_rx_byte, 1U);
}

static void cmd_parse(const char *cmd)
{
    float kp, ki, kd;
    int32_t val;
    uint32_t pct;

    if (sscanf(cmd, "PID %f %f %f", &kp, &ki, &kd) == 3) {
        speed_pid_t *pid = motor_ctrl_get_pid();
        pid_set_gains(pid, kp, ki, kd);
        BSP_UART_Printf("OK PID %.5f %.5f %.5f\r\n", kp, ki, kd);
    } else if (sscanf(cmd, "SPD %ld", &val) == 1) {
        motor_ctrl_set_speed_target(val);
        BSP_UART_Printf("OK SPD %ld\r\n", val);
    } else if (strcmp(cmd, "RUN") == 0) {
        if (!motor_ctrl_is_running()) {
            uint16_t duty = (BSP_PWM_PERIOD + 1U) * 5U / 100U;
            uint8_t dir = (motor_ctrl_get_speed_target() >= 0) ? MOTOR_DIR_CW : MOTOR_DIR_CCW;
            motor_start_simple(duty, dir);
        }
        BSP_UART_Printf("OK RUN\r\n");
    } else if (strcmp(cmd, "STP") == 0) {
        motor_stop();
        BSP_UART_Printf("OK STP\r\n");
    } else if (sscanf(cmd, "MOD %lu", &pct) == 1) {
        motor_ctrl_set_mode((uint8_t)pct);
        BSP_UART_Printf("OK MOD %lu\r\n", pct);
    } else if (sscanf(cmd, "DTY %lu", &pct) == 1) {
        uint16_t duty = (uint16_t)((BSP_PWM_PERIOD + 1U) * pct / 100U);
        motor_ctrl_set_duty(duty);
        BSP_UART_Printf("OK DTY %lu\r\n", pct);
    } else {
        BSP_UART_Printf("ERR %s\r\n", cmd);
    }
}

void bsp_cmd_process(void)
{
    char local_buf[CMD_BUF_SIZE];

    if (!s_cmd_ready) {
        return;
    }

    memcpy(local_buf, s_cmd_buf, s_cmd_len + 1U);
    s_cmd_len = 0U;
    s_cmd_ready = 0U;

    cmd_parse(local_buf);
}

uint8_t *bsp_cmd_get_rx_byte_ptr(void)
{
    return &s_rx_byte;
}
