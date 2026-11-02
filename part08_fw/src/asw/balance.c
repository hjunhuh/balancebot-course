/*
 * balance.c
 *
 * 상태 전이
 *   INIT -> CALIB : 즉시
 *   CALIB -> WAIT_UPRIGHT : 자이로 200 샘플 평균 완료 (흔들리면 재시작)
 *   WAIT_UPRIGHT, FALLEN -> BALANCE : |angle - trim| < 5 deg 가 300 ms 유지
 *   BALANCE -> FALLEN : |angle| > 45 deg
 *   (보정 이후 모든 상태) -> IDLE : motor_enable == 0
 *   IDLE -> WAIT_UPRIGHT : motor_enable == 1
 *   (보정 이후 모든 상태) -> LOW_BATT : 배터리 < 6.6 V 가 2 s 유지 (복귀 없음, 전원 재투입)
 *
 * 제어 법칙 (BALANCE)
 *   target = trim + (joy_y / 100) * MAX_LEAN - ks * (v_est / 255)
 *   u      = PID(angle - target, rate)
 *   left   = u + turn,  right = u - turn,  turn = (joy_x / 100) * MAX_TURN_PWM
 *   앞으로 넘어지면(angle > 0) u > 0 이 되어 바퀴가 앞으로 굴러 몸체 아래로 들어갑니다.
 */
#include <math.h>
#include "balance.h"
#include "config.h"

#define RAD2DEG 57.29577951f

static float absf(float v) { return (v < 0.0f) ? -v : v; }

static int16_t clamp_pwm(float v)
{
    if (v > (float)PWM_MAX) v = (float)PWM_MAX;
    if (v < -(float)PWM_MAX) v = -(float)PWM_MAX;
    return (int16_t)v;
}

static void enter(balance_t *b, bal_state_t s)
{
    b->st = s;
    b->timer_ms = 0;
    b->out = 0.0f;          /* 제어 상태가 아니면 출력 표시도 0 */
}

static void calib_restart(balance_t *b)
{
    b->calib_n = 0;
    b->calib_sum = 0.0f;
    b->calib_min = 1e9f;
    b->calib_max = -1e9f;
}

void balance_set_params(balance_t *b, const params_t *p)
{
    pid_set_gains(&b->pid, (float)p->kp / 100.0f, (float)p->ki / 100.0f, (float)p->kd / 100.0f);
    b->trim_deg = (float)p->trim / 100.0f;
    b->ks = (float)p->ks / 100.0f;
}

void balance_init(balance_t *b, const params_t *p)
{
    kalman_init(&b->kf, KF_Q_ANGLE, KF_Q_BIAS, KF_R_MEASURE);
    pid_init(&b->pid, (float)PWM_MAX);
    balance_set_params(b, p);
    b->lowbatt_ms = 0;
    b->gyro_bias = 0.0f;
    b->acc_angle = 0.0f;
    b->angle = 0.0f;
    b->rate = 0.0f;
    b->target = 0.0f;
    b->out = 0.0f;
    b->v_est = 0.0f;
    calib_restart(b);
    enter(b, BAL_INIT);
}

/* 세워진 상태가 일정 시간 유지되면 제어 시작 */
static void check_upright(balance_t *b)
{
    if (absf(b->angle - b->trim_deg) < UPRIGHT_ANGLE_DEG) {
        if (b->timer_ms >= UPRIGHT_HOLD_MS) {
            pid_reset(&b->pid);
            b->v_est = 0.0f;
            b->out = 0.0f;
            enter(b, BAL_BALANCE);
        }
    } else {
        b->timer_ms = 0;
    }
}

void balance_step(balance_t *b, const bal_input_t *in, bal_output_t *out)
{
    out->pwm_l = 0;
    out->pwm_r = 0;

    /* 가속도로 구한 기울기 (앞으로 기울면 +) */
    b->acc_angle = (float)IMU_ANGLE_SIGN * atan2f(-in->ax_g, in->az_g) * RAD2DEG;
    float gyro = (float)IMU_ANGLE_SIGN * in->gy_dps;

    if (b->timer_ms < 60000u) {
        b->timer_ms += CTRL_PERIOD_MS;
    }

    if (b->st == BAL_INIT) {
        calib_restart(b);
        enter(b, BAL_CALIB);
        return;
    }

    if (b->st == BAL_CALIB) {
        b->calib_sum += gyro;
        if (gyro < b->calib_min) b->calib_min = gyro;
        if (gyro > b->calib_max) b->calib_max = gyro;
        b->calib_n++;
        if ((b->calib_max - b->calib_min) > CALIB_STILL_DPS) {
            calib_restart(b);                      /* 움직였으므로 처음부터 */
            return;
        }
        if (b->calib_n >= CALIB_SAMPLES) {
            b->gyro_bias = b->calib_sum / (float)b->calib_n;
            kalman_set_angle(&b->kf, b->acc_angle);
            b->kf.bias = 0.0f;
            b->angle = b->acc_angle;
            enter(b, in->motor_enable ? BAL_WAIT_UPRIGHT : BAL_IDLE);
        }
        return;
    }

    /* 보정 이후: 항상 각도 추정 */
    b->angle = kalman_update(&b->kf, b->acc_angle, gyro - b->gyro_bias, CTRL_DT);
    b->rate = b->kf.rate;

#if LOWBATT_ENABLE
    if (b->st != BAL_LOW_BATT) {
        if (in->vbat_mv < LOWBATT_MV) {
            if (b->lowbatt_ms < 60000u) b->lowbatt_ms += CTRL_PERIOD_MS;
            if (b->lowbatt_ms >= LOWBATT_HOLD_MS) {
                enter(b, BAL_LOW_BATT);
            }
        } else {
            b->lowbatt_ms = 0;
        }
    }
#endif

    if (b->st == BAL_LOW_BATT) {
        return;                                    /* 모터 정지 유지 */
    }

    if (!in->motor_enable) {
        if (b->st != BAL_IDLE) enter(b, BAL_IDLE);
        return;
    }

    switch (b->st) {
    case BAL_IDLE:
        enter(b, BAL_WAIT_UPRIGHT);
        break;

    case BAL_WAIT_UPRIGHT:
    case BAL_FALLEN:
        check_upright(b);
        break;

    case BAL_BALANCE: {
        if (absf(b->angle) > FALL_ANGLE_DEG) {
            enter(b, BAL_FALLEN);
            break;
        }
        b->target = b->trim_deg
                  + ((float)in->joy_y / 100.0f) * MAX_LEAN_DEG
                  - b->ks * (b->v_est / (float)PWM_MAX);

        b->out = pid_update(&b->pid, b->angle - b->target, b->rate, CTRL_DT);
        b->v_est += VEL_LPF_ALPHA * (b->out - b->v_est);

        float turn = ((float)in->joy_x / 100.0f) * (float)MAX_TURN_PWM;
        out->pwm_l = clamp_pwm(b->out + turn);
        out->pwm_r = clamp_pwm(b->out - turn);
        break;
    }

    default:
        break;
    }
}
