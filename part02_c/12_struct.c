/*
 * 12_struct.c : 구조체, 센서 데이터 구조체, 구조체 포인터
 *
 * [학습 목표]
 *  1. struct 로 관련된 데이터를 하나로 묶고 typedef 로 이름을 붙입니다.
 *  2. 지정 초기화(.field = 값)와 구조체 대입(복사)을 사용합니다.
 *  3. 구조체 포인터와 -> 연산자로 함수가 구조체를 수정하게 만듭니다.
 *  4. 패딩 때문에 sizeof(구조체) 가 멤버 크기의 합보다 클 수 있음을 확인합니다.
 *  5. MPU6050 원시값과 PID 파라미터를 구조체로 모델링합니다.
 */

#include <stdio.h>
#include <stdint.h>
#include <stddef.h>   /* offsetof */

/* MPU6050 한 번 읽기의 원시값 (레지스터 0x3B 부터 14바이트) */
typedef struct {
    int16_t ax, ay, az;     /* 가속도 원시값, ±2 g 에서 16384 LSB/g */
    int16_t temp;           /* 온도 원시값 */
    int16_t gx, gy, gz;     /* 자이로 원시값, ±250 dps 에서 131 LSB/dps */
} ImuRaw;

/* PID 파라미터 (프로토콜 v1 처럼 모두 x100 정수) */
typedef struct {
    int32_t kp;
    int32_t ki;
    int32_t kd;
    int32_t trim;           /* 기준 각도 보정, x100 도 */
} PidParam;

/* PID 계산 중 바뀌는 상태 */
typedef struct {
    const PidParam *param;  /* 파라미터는 다른 곳에 있고 가리키기만 합니다 */
    int32_t integral;
    int32_t prev_error;
} PidState;

/* 패딩을 관찰하기 위한 구조체 */
struct Padded {
    uint8_t  a;   /* 1 */
    uint32_t b;   /* 4 : 4의 배수 주소에 놓이도록 a 뒤에 빈칸 3바이트 */
    uint8_t  c;   /* 1 : 구조체 전체 크기도 4의 배수가 되도록 뒤에 빈칸 */
};
struct Packed {
    uint32_t b;
    uint8_t  a;
    uint8_t  c;   /* 큰 멤버를 앞으로 모으면 빈칸이 줄어듭니다 */
};

/* 구조체를 값으로 받으면 통째로 복사됩니다. 읽기만 할 때는 const 포인터가 좋습니다. */
static void imu_print(const ImuRaw *r)
{
    printf("acc(%6d,%6d,%6d) gyro(%6d,%6d,%6d)\n",
           r->ax, r->ay, r->az, r->gx, r->gy, r->gz);
}

/* 구조체 포인터로 받아 내용을 수정합니다. r->gx 는 (*r).gx 와 같습니다. */
static void imu_remove_offset(ImuRaw *r, const ImuRaw *offset)
{
    r->gx = (int16_t)(r->gx - offset->gx);
    r->gy = (int16_t)(r->gy - offset->gy);
    r->gz = (int16_t)(r->gz - offset->gz);
}

/* 정수 PID 한 스텝. 결과는 모터 출력 범위로 제한합니다. */
static int16_t pid_step(PidState *s, int32_t angle_x100)
{
    int32_t error = s->param->trim - angle_x100;
    s->integral += error;
    int32_t deriv = error - s->prev_error;
    s->prev_error = error;

    int32_t out = (s->param->kp * error
                 + s->param->ki * s->integral / 100
                 + s->param->kd * deriv) / 10000;
    if (out > 255)  out = 255;
    if (out < -255) out = -255;
    return (int16_t)out;
}

int main(void)
{
    printf("== 1. struct and designated initializer ==\n");
    ImuRaw sample = { .ax = 120, .ay = -40, .az = 16300, .temp = -1200,
                      .gx = 85, .gy = -12, .gz = 30 };
    imu_print(&sample);
    printf("sizeof(ImuRaw) = %u (7 x int16_t)\n", (unsigned)sizeof(ImuRaw));

    printf("\n== 2. assignment copies all members ==\n");
    ImuRaw copy = sample;   /* 구조체는 대입으로 통째로 복사됩니다 (배열은 안 됩니다) */
    copy.gx = 0;
    printf("sample.gx=%d copy.gx=%d\n", sample.gx, copy.gx);

    printf("\n== 3. modify through pointer (->) ==\n");
    ImuRaw gyro_offset = { .gx = 80, .gy = -10, .gz = 25 };  /* 나머지는 0 */
    imu_remove_offset(&sample, &gyro_offset);
    imu_print(&sample);

    printf("\n== 4. array of structs ==\n");
    ImuRaw log[3] = {
        { .ax = 0,    .az = 16384, .gx = 131 },
        { .ax = 8192, .az = 14189, .gx = -262 },
        { .ax = -300, .az = 16380, .gx = 0 },
    };
    for (int i = 0; i < 3; i++) {
        /* 원시값을 물리량으로: 정수로만 계산 (x100) */
        long acc_x_g100 = (long)log[i].ax * 100 / 16384;
        long gyro_dps100 = (long)log[i].gx * 100 / 131;
        printf("log[%d] ax=%4ld (x0.01 g)  gx=%5ld (x0.01 dps)\n", i, acc_x_g100, gyro_dps100);
    }

    printf("\n== 5. struct holding a pointer: PID ==\n");
    PidParam param = { .kp = 2500, .ki = 10, .kd = 4000, .trim = 0 };
    PidState pid = { .param = &param, .integral = 0, .prev_error = 0 };
    /* 첫 스텝은 prev_error 가 0 이라 미분항이 크게 튀어 출력이 포화됩니다
       (derivative kick). 실제 펌웨어에서는 첫 스텝에 prev_error 를 현재 오차로
       맞추거나, 오차 대신 측정값의 변화량으로 미분합니다. */
    const int32_t angles[] = { 500, 300, 100, -50, -100 };
    for (int i = 0; i < 5; i++) {
        int16_t out = pid_step(&pid, angles[i]);
        printf("angle=%5ld -> out=%4d (integral=%ld)\n",
               (long)angles[i], out, (long)pid.integral);
    }
    param.kp = 5000;   /* 파라미터를 바꾸면 pid 가 가리키므로 바로 반영됩니다 */
    printf("after kp change: out=%d\n", pid_step(&pid, -100));

    printf("\n== 6. padding ==\n");
    printf("struct Padded: sizeof=%u offsets a=%u b=%u c=%u\n",
           (unsigned)sizeof(struct Padded), (unsigned)offsetof(struct Padded, a),
           (unsigned)offsetof(struct Padded, b), (unsigned)offsetof(struct Padded, c));
    printf("struct Packed: sizeof=%u offsets b=%u a=%u c=%u\n",
           (unsigned)sizeof(struct Packed), (unsigned)offsetof(struct Packed, b),
           (unsigned)offsetof(struct Packed, a), (unsigned)offsetof(struct Packed, c));
    /* AVR 은 8비트 CPU 라 정렬 제약이 없어 둘 다 6 바이트가 됩니다.
       같은 구조체도 CPU 마다 크기가 다를 수 있으므로, 통신 프레임이나 센서 바이트를
       구조체에 그대로 memcpy 해서 해석하는 방식은 피합니다. */

    return 0;
}

/*
 * [연습 문제]
 *  1. 배터리 정보를 담는 구조체 Battery { uint16_t mv; uint8_t percent; uint8_t low; }
 *     를 만들고, mv 를 받아 나머지 필드를 채우는 함수 battery_update(Battery *b, uint16_t mv)
 *     를 작성해 보세요 (6400 mV = 0 %, 8400 mV = 100 %).
 *  2. struct Padded 의 멤버 순서를 바꿔 sizeof 가 8 이 되는 배치를 찾아보세요.
 *  3. pid_step 에 적분값 제한(anti windup, 예 ±100000)을 추가해 보세요.
 */
