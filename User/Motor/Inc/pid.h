/**
 * @file    pid.h
 * @brief   增量式PID控制器
 */

#ifndef __PID_H
#define __PID_H

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    float SetPoint;     /* 目标值 (RPM, 有符号) */
    float ActualValue;  /* 累积输出 */
    float Kp, Ki, Kd;
    float Error;        /* e(k) */
    float LastError;    /* e(k-1) */
    float PrevError;    /* e(k-2) */
    float OutMin, OutMax;
} speed_pid_t;

void  pid_init(speed_pid_t *pid, float kp, float ki, float kd);
void  pid_reset(speed_pid_t *pid);
void  pid_set_limits(speed_pid_t *pid, float out_min, float out_max);
void  pid_set_gains(speed_pid_t *pid, float kp, float ki, float kd);
float pid_compute(speed_pid_t *pid, float measured);

#ifdef __cplusplus
}
#endif

#endif /* __PID_H */
