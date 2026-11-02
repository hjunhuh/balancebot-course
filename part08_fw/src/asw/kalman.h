/*
 * kalman.h
 * 1축 기울기 추정용 2상태 칼만 필터 (상태: 각도, 자이로 바이어스).
 * 하드웨어 의존성이 없어 PC 에서 단위 테스트합니다.
 */
#ifndef KALMAN_H
#define KALMAN_H

typedef struct {
    float angle;      /* 추정 각도 [deg] */
    float bias;       /* 추정 자이로 바이어스 [deg/s] */
    float rate;       /* 바이어스를 뺀 각속도 [deg/s] */
    float P[2][2];    /* 오차 공분산 */
    float q_angle;    /* 각도 프로세스 잡음 */
    float q_bias;     /* 바이어스 프로세스 잡음 */
    float r_measure;  /* 가속도 각도 측정 잡음 */
} kalman_t;

void  kalman_init(kalman_t *k, float q_angle, float q_bias, float r_measure);
void  kalman_set_angle(kalman_t *k, float angle);
/* new_angle: 가속도로 구한 각도 [deg], new_rate: 자이로 각속도 [deg/s], dt [s] */
float kalman_update(kalman_t *k, float new_angle, float new_rate, float dt);

#endif
