#include "control/pid.h"

void pid_init(pid_t *pid, float Kp, float Ki, float Kd,
              float dt, float out_min, float out_max)
{
    pid->Kp       = Kp;
    pid->Ki       = Ki;
    pid->Kd       = Kd;
    pid->dt       = dt;
    pid->out_min  = out_min;
    pid->out_max  = out_max;
    pid->integral = 0.0f;
    pid->prev_error = 0.0f;
    pid->output   = 0.0f;
}

float pid_step(pid_t *pid, float setpoint, float feedback)
{
    float error = setpoint - feedback;

    float p_term = pid->Kp * error;
    float i_term = pid->integral + pid->Ki * error * pid->dt;
    float d_term = pid->Kd * (error - pid->prev_error) / pid->dt;

    pid->prev_error = error;

    float out = p_term + i_term + d_term;

    // Clamp output
    if (out > pid->out_max) {
        out = pid->out_max;
    } else if (out < pid->out_min) {
        out = pid->out_min;
    } else {
        // Anti-windup: only integrate when output is not in saturation
        pid->integral = i_term;
    }

    pid->output = out;
    return out;
}

void pid_reset(pid_t *pid)
{
    pid->integral   = 0.0f;
    pid->prev_error = 0.0f;
    pid->output     = 0.0f;
}
