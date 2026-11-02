/*
 * kalman.c
 *
 * 상태 x = [angle, bias]^T
 * 예측:  angle = angle + dt * (gyro - bias)
 *        P = A P A^T + Q
 * 보정:  y = acc_angle - angle
 *        S = P00 + R,  K = [P00 / S, P10 / S]
 *        x = x + K y,  P = (I - K H) P
 */
#include "kalman.h"

void kalman_init(kalman_t *k, float q_angle, float q_bias, float r_measure)
{
    k->angle = 0.0f;
    k->bias = 0.0f;
    k->rate = 0.0f;
    k->P[0][0] = 0.0f;
    k->P[0][1] = 0.0f;
    k->P[1][0] = 0.0f;
    k->P[1][1] = 0.0f;
    k->q_angle = q_angle;
    k->q_bias = q_bias;
    k->r_measure = r_measure;
}

void kalman_set_angle(kalman_t *k, float angle)
{
    k->angle = angle;
}

float kalman_update(kalman_t *k, float new_angle, float new_rate, float dt)
{
    /* 1) 예측 */
    k->rate = new_rate - k->bias;
    k->angle += dt * k->rate;

    k->P[0][0] += dt * (dt * k->P[1][1] - k->P[0][1] - k->P[1][0] + k->q_angle);
    k->P[0][1] -= dt * k->P[1][1];
    k->P[1][0] -= dt * k->P[1][1];
    k->P[1][1] += k->q_bias * dt;

    /* 2) 보정 */
    float s  = k->P[0][0] + k->r_measure;
    float k0 = k->P[0][0] / s;
    float k1 = k->P[1][0] / s;

    float y = new_angle - k->angle;
    k->angle += k0 * y;
    k->bias  += k1 * y;

    float p00 = k->P[0][0];
    float p01 = k->P[0][1];
    k->P[0][0] -= k0 * p00;
    k->P[0][1] -= k0 * p01;
    k->P[1][0] -= k1 * p00;
    k->P[1][1] -= k1 * p01;

    return k->angle;
}
