/*
 * balance.h
 * 밸런싱 제어 ASW: 상태 머신 + 칼만 필터 + PID.
 * 입력은 물리 단위(g, deg/s, mV), 출력은 좌우 모터 PWM(-255..255).
 * 레지스터를 전혀 모르기 때문에 PC 시뮬레이터에서 그대로 검증할 수 있습니다.
 */
#ifndef BALANCE_H
#define BALANCE_H

#include <stdint.h>
#include "kalman.h"
#include "pid.h"
#include "params.h"

typedef enum {
    BAL_INIT = 0,
    BAL_CALIB = 1,
    BAL_WAIT_UPRIGHT = 2,
    BAL_BALANCE = 3,
    BAL_FALLEN = 4,
    BAL_IDLE = 5,
    BAL_LOW_BATT = 6
} bal_state_t;

typedef struct {
    float    ax_g;          /* 전후 방향 가속도 [g] */
    float    az_g;          /* 상하 방향 가속도 [g] */
    float    gy_dps;        /* 피치 축 자이로 [deg/s], 바이어스 포함 원시값 */
    uint16_t vbat_mv;
    int8_t   joy_x;         /* -100..100, +오른쪽 회전 */
    int8_t   joy_y;         /* -100..100, +전진 */
    uint8_t  motor_enable;
} bal_input_t;

typedef struct {
    int16_t pwm_l;
    int16_t pwm_r;
} bal_output_t;

typedef struct {
    bal_state_t st;
    uint16_t    timer_ms;       /* 상태별 경과 시간 */
    uint16_t    lowbatt_ms;
    uint16_t    calib_n;
    float       calib_sum;
    float       calib_min;
    float       calib_max;
    float       gyro_bias;      /* 보정으로 구한 초기 바이어스 */
    kalman_t    kf;
    pid_ctrl_t  pid;
    float       acc_angle;
    float       angle;          /* 필터 출력 각도 [deg] */
    float       rate;           /* 각속도 [deg/s] */
    float       target;         /* 목표 각도 [deg] */
    float       out;            /* PID 출력 */
    float       v_est;          /* 출력 기반 속도 추정 (PWM 단위) */
    float       trim_deg;
    float       ks;
} balance_t;

void balance_init(balance_t *b, const params_t *p);
void balance_set_params(balance_t *b, const params_t *p);
void balance_step(balance_t *b, const bal_input_t *in, bal_output_t *out);

#endif
