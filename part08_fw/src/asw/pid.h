/*
 * pid.h
 * 이산 PID 제어기. 미분항은 오차를 미분하지 않고 측정 각속도를 바로 씁니다
 * (목표값이 바뀔 때 미분 튐 방지, 자이로가 이미 깨끗한 미분값을 줌).
 * 이름을 pid_t 로 하지 않는 이유: PC 의 sys/types.h 에 pid_t 가 이미 있습니다.
 */
#ifndef PID_H
#define PID_H

typedef struct {
    float kp;
    float ki;
    float kd;
    float integ;        /* 오차 적분값 [deg*s] */
    float integ_limit;  /* 적분 포화 한계 [deg*s] */
    float out_limit;    /* 출력 한계 */
} pid_ctrl_t;

void  pid_init(pid_ctrl_t *p, float out_limit);
void  pid_set_gains(pid_ctrl_t *p, float kp, float ki, float kd);
void  pid_reset(pid_ctrl_t *p);
/* error: 측정 - 목표, d_meas: 측정값의 미분 (각속도) */
float pid_update(pid_ctrl_t *p, float error, float d_meas, float dt);

#endif
