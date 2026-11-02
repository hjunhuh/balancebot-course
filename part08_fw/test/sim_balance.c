/*
 * sim_balance.c
 * 실제 펌웨어의 ASW(balance.c, kalman.c, pid.c) 와 모터 데드존 매핑을 그대로 링크해서
 * 단순화한 도립진자 + DC 모터 모델 위에서 돌려 보는 PC 시뮬레이터입니다.
 * 목적: 부호 규칙(앞으로 기울면 앞으로 구름), 상태 전이, 게인의 대략적 크기 검증.
 * 실제 로봇은 질량, 마찰, 백래시가 달라 Part 9 의 튜닝 절차로 게인을 다시 찾습니다.
 *
 * 모델
 *   theta'' = (g sin(theta) - a cos(theta)) / L      (L: 바퀴축에서 무게중심까지)
 *   v' = (v_cmd - v) / TAU,  v_cmd = VMAX * 유효듀티  (PWM 으로 속도를 정하는 DC 모터 근사)
 *   유효듀티 = max(0, duty - STICTION) / (255 - STICTION)  (정지마찰 데드존)
 *   가속도계는 중력 + 차체 가속도를 함께 측정, 자이로는 바이어스와 잡음 포함
 *
 * 사용: ./sim_balance [kp ki kd ks]   (게인은 실수, 생략하면 config.h 기본값)
 */
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include "balance.h"
#include "params.h"
#include "config.h"
#include "motor.h"

#define G        9.81
static double L    = 0.07;   /* m, 바퀴축에서 무게중심까지, Part 6 설계값 약 69 mm (SIM_L) */
static double TAU  = 0.10;   /* s, 모터 속도 응답 시정수 (SIM_TAU) */
static double VMAX = 1.2;    /* m/s, PWM 255 일 때 바퀴 선속도 (SIM_VMAX) */
#define STICTION 35.0
#define DT_PHYS  0.001
#define H_IMU    0.08      /* m, 바퀴축에서 IMU 까지 높이 */
#define PI_D     3.14159265358979
#define DEG      (180.0 / PI_D)

typedef struct { double th, w, x, v; } plant_t;
static int g_trace = 0;
static int g_ideal = 0;   /* 1: 센서가 참값을 그대로 줌 (제어 법칙만 검증) */

static double frand(double amp) { return amp * ((double)rand() / RAND_MAX * 2.0 - 1.0); }

static double wheel_speed_cmd(int16_t cmd)
{
    double d = (double)motor_map_duty(cmd);
    double eff = (d - STICTION) / (255.0 - STICTION);
    if (eff < 0.0) eff = 0.0;
    return (cmd >= 0 ? 1.0 : -1.0) * eff * VMAX;
}

static double g_alpha = 0.0;

static void plant_step(plant_t *p, double v_cmd, double *acc)
{
    double a = (v_cmd - p->v) / TAU;
    double alpha = (G * sin(p->th) - a * cos(p->th)) / L;
    g_alpha = alpha;
    p->w += alpha * DT_PHYS;
    p->th += p->w * DT_PHYS;
    p->v += a * DT_PHYS;
    p->x += p->v * DT_PHYS;
    *acc = a;
}

static void sense(const plant_t *p, double a, bal_input_t *in)
{
    double s = sin(p->th), c = cos(p->th);
    if (g_ideal) {
        in->ax_g = (float)(-s);
        in->az_g = (float)c;
        in->gy_dps = (float)(p->w * DEG);
        in->vbat_mv = 7600;
        return;
    }
    /* 차체 가속 + IMU 위치의 접선/구심 가속도까지 측정됨 */
    in->ax_g = (float)((a / G) * c - s + H_IMU * g_alpha / G + frand(0.02));
    in->az_g = (float)((a / G) * s + c - H_IMU * p->w * p->w / G + frand(0.02));
    in->gy_dps = (float)(p->w * DEG + 1.2 + frand(0.3));
    in->vbat_mv = 7600;
}

typedef struct {
    double t_end;       /* 시뮬 시간 */
    int8_t joy_y;
    double push_t;      /* 이 시각에 각속도 충격 */
    double push_dps;
} scenario_t;

/* 반환 1: 시나리오 동안 넘어지지 않음 */
static int run(const params_t *prm, const char *name, const scenario_t *sc, int expect_fall)
{
    balance_t b;
    plant_t p = { 3.0 / DEG, 0, 0, 0 };
    bal_input_t in = { 0 };
    bal_output_t out = { 0, 0 };
    double acc = 0.0;
    int held = 1, fell = 0;
    double t = 0.0, t_release = 0.0, max_th = 0.0, max_x = 0.0, v_sum = 0.0;
    long v_n = 0;
    int pushed = 0;

    balance_init(&b, prm);
    in.motor_enable = 1;

    for (long k = 0; ; k++) {
        t = k * DT_PHYS;
        if (!held && t - t_release > sc->t_end) break;
        if (held && t > 5.0) { printf("  [%s] BALANCE 상태 진입 실패\n", name); return 0; }

        if (k % 5 == 0) {                       /* 5 ms 제어 주기 */
            in.joy_x = 0;
            in.joy_y = (!held && t - t_release > 2.0) ? sc->joy_y : 0;
            sense(&p, acc, &in);
            balance_step(&b, &in, &out);
            if (held && b.st == BAL_BALANCE) { held = 0; t_release = t; }
            if (!held && b.st == BAL_FALLEN && !fell) {
                fell = 1;
                printf("  [%s] t=%.2fs 넘어짐 판정 (theta %.1f deg)\n", name, t - t_release, p.th * DEG);
            }
        }
        if (held) { p.w = 0; p.v = 0; acc = 0; continue; }   /* 손으로 잡고 있음 */

        if (!pushed && sc->push_dps != 0.0 && t - t_release >= sc->push_t) {
            p.w += sc->push_dps / DEG;
            pushed = 1;
        }
        double vl = wheel_speed_cmd(out.pwm_l), vr = wheel_speed_cmd(out.pwm_r);
        plant_step(&p, 0.5 * (vl + vr), &acc);
        if (fabs(p.th) > PI_D / 2) { p.th = (p.th > 0 ? 1 : -1) * PI_D / 2; p.w = 0; p.v = 0; }

        double tr = t - t_release;
        if (g_trace && k % 50 == 0)
            printf("T %.2f th %+.2f w %+.1f v %+.3f x %+.3f out %+.0f tgt %+.2f est %+.2f vest %+.0f st %d\n",
                   tr, p.th * DEG, p.w * DEG, p.v, p.x, b.out, b.target, b.angle, b.v_est, (int)b.st);
        if (tr > 1.0 && fabs(p.th * DEG) > max_th) max_th = fabs(p.th * DEG);
        if (fabs(p.x) > max_x) max_x = fabs(p.x);
        if (tr > sc->t_end - 2.0) { v_sum += p.v; v_n++; }
    }
    int ok = !fell;
    if (expect_fall) {
        printf("  [%s] %s\n", name, fell ? "OK   넘어짐을 감지하고 모터 정지" : "FAIL 넘어짐 미감지");
    } else {
        printf("  [%s] %s  max|theta| %.2f deg, max|x| %.2f m, 마지막 2s 평균속도 %+.3f m/s\n",
               name, ok ? "OK  " : "FAIL", max_th, max_x, v_n ? v_sum / v_n : 0.0);
    }
    return ok;
}

int main(int argc, char **argv)
{
    params_t prm;
    params_default(&prm);
    if (argc >= 4) {
        prm.kp = (int32_t)lround(atof(argv[1]) * 100);
        prm.ki = (int32_t)lround(atof(argv[2]) * 100);
        prm.kd = (int32_t)lround(atof(argv[3]) * 100);
        if (argc >= 5) prm.ks = (int32_t)lround(atof(argv[4]) * 100);
    }
    printf("gains: Kp %.2f  Ki %.2f  Kd %.2f  Ks %.2f\n",
           prm.kp / 100.0, prm.ki / 100.0, prm.kd / 100.0, prm.ks / 100.0);
    srand(42);
    g_trace = getenv("SIM_TRACE") != NULL;
    g_ideal = getenv("SIM_IDEAL") != NULL;
    if (getenv("SIM_L"))    L    = atof(getenv("SIM_L"));
    if (getenv("SIM_TAU"))  TAU  = atof(getenv("SIM_TAU"));
    if (getenv("SIM_VMAX")) VMAX = atof(getenv("SIM_VMAX"));

    scenario_t s1 = { 10.0, 0, 0, 0 };
    scenario_t s2 = { 8.0, 0, 3.0, 60.0 };
    scenario_t s3 = { 8.0, 60, 0, 0 };
    scenario_t s4 = { 4.0, 0, 1.0, 400.0 };
    int ok = 1;
    ok &= run(&prm, "제자리 10s", &s1, 0);
    ok &= run(&prm, "충격 60dps", &s2, 0);
    ok &= run(&prm, "전진 y=60", &s3, 0);
    int fell_expected = !run(&prm, "큰 충격 400dps", &s4, 1);
    printf("result: %s\n", (ok && fell_expected) ? "PASS" : "FAIL");
    return (ok && fell_expected) ? 0 : 1;
}
