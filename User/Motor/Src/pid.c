/**
 * @file    pid.c
 * @brief   增量式PID控制器实现
 */

#include "pid.h"

void pid_init(speed_pid_t *pid, float kp, float ki, float kd)
{
    pid->SetPoint    = 0.0f;
    pid->ActualValue = 0.0f;
    pid->Kp          = kp;
    pid->Ki          = ki;
    pid->Kd          = kd;
    pid->Error       = 0.0f;
    pid->LastError   = 0.0f;
    pid->PrevError   = 0.0f;
    pid->OutMin      = -8399.0f;
    pid->OutMax      =  8399.0f;
}

void pid_reset(speed_pid_t *pid)
{
    pid->ActualValue = 0.0f;
    pid->Error       = 0.0f;
    pid->LastError   = 0.0f;
    pid->PrevError   = 0.0f;
}

void pid_set_limits(speed_pid_t *pid, float out_min, float out_max)
{
    pid->OutMin = out_min;
    pid->OutMax = out_max;
}

void pid_set_gains(speed_pid_t *pid, float kp, float ki, float kd)
{
    pid->Kp = kp;
    pid->Ki = ki;
    pid->Kd = kd;
}

float pid_compute(speed_pid_t *pid, float measured)
{
    float delta;

    pid->PrevError = pid->LastError;
    pid->LastError = pid->Error;
    pid->Error     = pid->SetPoint - measured;

    delta = pid->Kp * (pid->Error - pid->LastError)
          + pid->Ki *  pid->Error
          + pid->Kd * (pid->Error - 2.0f * pid->LastError + pid->PrevError);

    pid->ActualValue += delta;

    /* 输出限幅 */
    if (pid->ActualValue > pid->OutMax) {
        pid->ActualValue = pid->OutMax;
    } else if (pid->ActualValue < pid->OutMin) {
        pid->ActualValue = pid->OutMin;
    }

    return pid->ActualValue;
}
