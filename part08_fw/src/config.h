/*
 * config.h
 * 밸런싱 로봇 펌웨어 전역 설정값.
 * 하드웨어 조립 방향이 다르면 이 파일의 부호/반전 매크로만 바꿉니다.
 * (Part 9 통합 테스트 TC-03, TC-04 에서 부호를 확인합니다)
 */
#ifndef CONFIG_H
#define CONFIG_H

/* ---------- 타이밍 ---------- */
#define CTRL_PERIOD_MS        5u       /* 제어 주기 5 ms = 200 Hz */
#define CTRL_DT               0.005f   /* 제어 주기 [s] */
#define SLOW_PERIOD_MS        100u     /* 텔레메트리, 배터리, LED 주기 */

/* ---------- IMU 장착 방향 ----------
 * 기준: MPU6050 을 중판에 수평으로 두고 X 축이 로봇 앞쪽을 향함.
 * 앞으로 기울 때 각도가 + 가 되어야 합니다. 반대로 나오면 -1 로 바꿉니다. */
#define IMU_ANGLE_SIGN        (1)

/* ---------- 모터 ---------- */
#define MOTOR_L_INVERT        0        /* 왼쪽 모터가 반대로 돌면 1 */
#define MOTOR_R_INVERT        0        /* 오른쪽 모터가 반대로 돌면 1 */
#define MOTOR_DEADBAND_PWM    40       /* 바퀴가 돌기 시작하는 최소 PWM */
#define PWM_MAX               255

/* ---------- 밸런싱 동작 ---------- */
#define FALL_ANGLE_DEG        45.0f    /* 이 각도를 넘으면 넘어짐으로 판정 */
#define UPRIGHT_ANGLE_DEG     5.0f     /* 세워졌다고 판단하는 범위 */
#define UPRIGHT_HOLD_MS       300u     /* 세워진 상태 유지 시간 */
#define MAX_LEAN_DEG          3.0f     /* 조이스틱 최대 전진 시 목표 기울기 */
#define MAX_TURN_PWM          60       /* 조이스틱 최대 회전 시 좌우 PWM 차 */
#define DRIVE_TIMEOUT_MS      500u     /* 조이스틱 명령 수신 끊김 판정 */
#define VEL_LPF_ALPHA         0.02f    /* 출력 기반 속도 추정 저역통과 계수 */

/* ---------- 보정 ---------- */
#define CALIB_SAMPLES         200u     /* 자이로 바이어스 평균 샘플 수 (1 s) */
#define CALIB_STILL_DPS       4.0f     /* 보정 중 이 범위 이상 흔들리면 재시작 */

/* ---------- 칼만 필터 ----------
 * 흔히 인터넷 예제에 있는 Q_BIAS 0.003, R 0.03 은 가속도계를 강하게 믿습니다.
 * 밸런싱 로봇은 기운 채로 가속하면 가속도계가 "똑바로 서 있다"고 착각하므로
 * (가짜 평형), 자이로를 더 믿도록 R 을 키우고 Q_BIAS 를 줄였습니다.
 * 근거: test/sweep_kalman.sh 에서 예제값은 324 경우 중 2 개 성공,
 *       아래 값은 200 개 성공 (Part 8 문서 8-14 참고). */
#ifndef KF_Q_ANGLE
#define KF_Q_ANGLE            0.001f
#endif
#ifndef KF_Q_BIAS
#define KF_Q_BIAS             0.0001f
#endif
#ifndef KF_R_MEASURE
#define KF_R_MEASURE          3.0f
#endif

/* ---------- 배터리 ---------- */
#define VBAT_DIV_RATIO        3u       /* (20k + 10k) / 10k */
#define VBAT_ADC_REF_MV       5000u
#define LOWBATT_ENABLE        1
#define LOWBATT_MV            6600u    /* 2S 리튬이온 셀당 3.3 V */
#define LOWBATT_HOLD_MS       2000u

/* ---------- 기본 파라미터 (x100 정수) ---------- */
#define DEFAULT_KP            1500     /* 15.00 PWM/deg      */
#define DEFAULT_KI            10000    /* 100.00 PWM/(deg*s) */
#define DEFAULT_KD            25       /*  0.25 PWM/(deg/s)  */
#define DEFAULT_TRIM          0        /*  0.00 deg          */
#define DEFAULT_KS            500      /*  5.00 deg          */

#endif /* CONFIG_H */
