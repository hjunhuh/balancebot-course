/* test_pid_kalman.c : PID 와 칼만 필터 기본 동작 테스트 */
#include <stdlib.h>
#include "test_util.h"
#include "pid.h"
#include "kalman.h"
#include "config.h"

static float noise(float amp)
{
    return amp * ((float)rand() / (float)RAND_MAX * 2.0f - 1.0f);
}

int main(void)
{
    /* PID */
    pid_ctrl_t p;
    pid_init(&p, 255.0f);
    pid_set_gains(&p, 20.0f, 0.0f, 1.0f);
    CHECK_NEAR(pid_update(&p, 2.0f, 10.0f, 0.005f), 50.0f, 1e-4);   /* 20*2 + 1*10 */
    CHECK_NEAR(pid_update(&p, 100.0f, 0.0f, 0.005f), 255.0f, 1e-4);  /* 출력 제한 */

    pid_set_gains(&p, 0.0f, 100.0f, 0.0f);       /* integ_limit = 2.55 */
    pid_reset(&p);
    for (int i = 0; i < 10000; i++) pid_update(&p, 10.0f, 0.0f, 0.005f);
    CHECK_NEAR(p.integ, 2.55f, 1e-3);            /* 적분 포화 방지 */
    float u = 0.0f;
    for (int i = 0; i < 20; i++) u = pid_update(&p, -10.0f, 0.0f, 0.005f);
    CHECK(u < 255.0f);                           /* 반대 오차에 바로 반응 */

    /* 칼만 1: 정지 상태 10 도. 펌웨어는 부팅 시 자이로 바이어스를 먼저 평균으로 빼므로
     * 필터에 들어가는 잔여 바이어스는 작습니다 (여기서는 0.2 deg/s 로 가정). */
    kalman_t k;
    kalman_init(&k, KF_Q_ANGLE, KF_Q_BIAS, KF_R_MEASURE);
    kalman_set_angle(&k, 8.0f);
    srand(1);
    for (int i = 0; i < 2000; i++) {             /* 10 s */
        kalman_update(&k, 10.0f + noise(1.0f), 0.2f + noise(0.3f), 0.005f);
    }
    printf("  kalman static: angle %.3f deg, bias %.3f deg/s\n", k.angle, k.bias);
    CHECK_NEAR(k.angle, 10.0f, 0.5f);

    /* 칼만 2: 30 deg/s 로 회전, 가속도 각도 잡음 +-5 deg */
    float truth = 10.0f, maxerr = 0.0f;
    for (int i = 0; i < 400; i++) {              /* 2 s */
        truth += 30.0f * 0.005f;
        float est = kalman_update(&k, truth + noise(5.0f), 30.0f + 0.2f + noise(0.3f), 0.005f);
        float e = fabsf(est - truth);
        if (e > maxerr) maxerr = e;
    }
    printf("  kalman ramp: max error %.3f deg (acc noise +-5 deg)\n", maxerr);
    CHECK(maxerr < 1.0f);

    /* 칼만 3: 가짜 평형. 실제로 5 도 기울어진 채 앞으로 가속하면 가속도계는 0 도를 가리킵니다.
     * 자이로를 충분히 믿는 설정이면 1 초 동안 추정값이 크게 끌려가지 않아야 합니다. */
    kalman_init(&k, KF_Q_ANGLE, KF_Q_BIAS, KF_R_MEASURE);
    kalman_set_angle(&k, 5.0f);
    for (int i = 0; i < 200; i++) {              /* 1 s */
        kalman_update(&k, 0.0f + noise(0.5f), 0.0f + noise(0.3f), 0.005f);
    }
    printf("  kalman fake-equilibrium: truth 5.00, estimate after 1 s %.3f deg\n", k.angle);
    CHECK(k.angle > 4.0f);

    TEST_REPORT("pid_kalman");
}
