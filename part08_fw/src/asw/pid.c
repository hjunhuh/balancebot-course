/*
 * pid.c
 * u = Kp * e + Ki * integral(e) + Kd * d(meas)/dt
 * 적분 포화(windup) 방지: Ki * integ 가 출력 한계를 넘지 않도록 적분값을 제한합니다.
 */
#include "pid.h"

static float clampf(float v, float lo, float hi)
{
    if (v < lo) return lo;
    if (v > hi) return hi;
    return v;
}

void pid_init(pid_ctrl_t *p, float out_limit)
{
    p->kp = 0.0f;
    p->ki = 0.0f;
    p->kd = 0.0f;
    p->integ = 0.0f;
    p->integ_limit = 0.0f;
    p->out_limit = out_limit;
}

void pid_set_gains(pid_ctrl_t *p, float kp, float ki, float kd)
{
    p->kp = kp;
    p->ki = ki;
    p->kd = kd;
    if (ki > 0.0f) {
        p->integ_limit = p->out_limit / ki;
    } else {
        p->integ_limit = 0.0f;
    }
    p->integ = clampf(p->integ, -p->integ_limit, p->integ_limit);
}

void pid_reset(pid_ctrl_t *p)
{
    p->integ = 0.0f;
}

float pid_update(pid_ctrl_t *p, float error, float d_meas, float dt)
{
    p->integ += error * dt;
    p->integ = clampf(p->integ, -p->integ_limit, p->integ_limit);

    float u = p->kp * error + p->ki * p->integ + p->kd * d_meas;
    return clampf(u, -p->out_limit, p->out_limit);
}
