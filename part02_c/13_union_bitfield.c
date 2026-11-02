/*
 * 13_union_bitfield.c : 공용체, 바이트 분해, 빅엔디언 결합, 비트필드
 *
 * [학습 목표]
 *  1. union 은 모든 멤버가 같은 메모리를 공유한다는 것을 이해합니다.
 *  2. union 으로 16비트, 32비트 값을 바이트로 나누어 보고 엔디언을 확인합니다.
 *  3. MPU6050 처럼 상위 바이트가 먼저 오는(빅엔디언) 데이터를
 *     시프트와 OR 로 int16_t 로 조합합니다 (엔디언과 무관한 안전한 방법).
 *  4. 비트필드로 레지스터 구조를 표현해 보고, 그 한계(이식성)를 압니다.
 */

#include <stdio.h>
#include <stdint.h>

/* ---------- 1. union ----------
 * 크기는 가장 큰 멤버의 크기입니다. 한 멤버에 쓰고 다른 멤버로 읽으면
 * 같은 바이트를 다른 모양으로 보게 됩니다. (C99 에서 허용되는 type punning) */
typedef union {
    uint16_t word;
    uint8_t  bytes[2];
} Split16;

typedef union {
    uint32_t value;
    uint8_t  bytes[4];
} Split32;

/* ---------- 2. 빅엔디언 2바이트를 int16_t 로 ----------
 * MPU6050 은 ACCEL_XOUT_H(0x3B), ACCEL_XOUT_L(0x3C) 순서로 상위 바이트를 먼저 보냅니다.
 * 두 바이트를 시프트로 합치면 CPU 의 엔디언과 상관없이 항상 같은 결과가 나옵니다.
 * 먼저 uint16_t 로 합친 뒤 int16_t 로 바꾸어 음수(2의 보수)를 올바르게 얻습니다. */
static int16_t be16_to_i16(const uint8_t *p)
{
    return (int16_t)(((uint16_t)p[0] << 8) | (uint16_t)p[1]);
}

/* ---------- 3. 비트필드 ----------
 * MPU6050 의 GYRO_CONFIG(0x1B) 레지스터 모양:
 *   bit7 XG_ST, bit6 YG_ST, bit5 ZG_ST, bit4..3 FS_SEL, bit2..0 (예약)
 * 비트필드는 "낮은 비트부터 채운다" 는 것이 gcc(그리고 avr-gcc)의 규칙이지만
 * C 표준은 순서를 정하지 않았습니다. 그래서 실제 레지스터 접근에는
 * 04장의 마스크와 시프트를 쓰고, 비트필드는 개념 모델과 내부 플래그 저장에 씁니다. */
typedef union {
    uint8_t reg;
    struct {
        uint8_t reserved : 3;   /* bit 0..2 */
        uint8_t fs_sel   : 2;   /* bit 3..4 : 0=±250, 1=±500, 2=±1000, 3=±2000 dps */
        uint8_t zg_st    : 1;   /* bit 5 */
        uint8_t yg_st    : 1;   /* bit 6 */
        uint8_t xg_st    : 1;   /* bit 7 */
    } bits;
} GyroConfig;

/* 로봇의 여러 상태 플래그를 1바이트에 담는 예 (이건 비트필드가 잘 맞는 용도입니다) */
typedef struct {
    uint8_t motor_enable : 1;
    uint8_t low_battery  : 1;
    uint8_t imu_ok       : 1;
    uint8_t bt_timeout   : 1;
    uint8_t state        : 4;   /* 0..15, 상태 번호 0..6 을 담기에 충분 */
} RobotFlags;

static void print_bytes(const char *label, const uint8_t *b, int n)
{
    printf("%-16s", label);
    for (int i = 0; i < n; i++) {
        printf(" [%d]=0x%02X", i, (unsigned)b[i]);
    }
    printf("\n");
}

int main(void)
{
    printf("== 1. union shares memory ==\n");
    printf("sizeof(Split16)=%u sizeof(Split32)=%u\n", (unsigned)sizeof(Split16), (unsigned)sizeof(Split32));
    Split16 s16;
    s16.word = 0x1234;
    print_bytes("0x1234 bytes:", s16.bytes, 2);
    printf("this PC is %s endian\n", (s16.bytes[0] == 0x34) ? "LITTLE" : "BIG");
    /* x86 과 AVR 은 모두 리틀엔디언입니다: 낮은 주소에 하위 바이트가 놓입니다. */

    Split32 s32;
    s32.value = 0xDEADBEEF;
    print_bytes("0xDEADBEEF:", s32.bytes, 4);

    printf("\n== 2. build 16-bit from bytes via union ==\n");
    s16.bytes[0] = 0x18;   /* 하위 */
    s16.bytes[1] = 0xFC;   /* 상위 */
    printf("bytes {0x18,0xFC} as word = 0x%04X (%d)\n",
           (unsigned)s16.word, (int16_t)s16.word);
    /* 이 방식은 CPU 엔디언에 따라 결과가 달라집니다. */

    printf("\n== 3. MPU6050 big endian burst read ==\n");
    /* 0x3B 부터 읽은 14바이트 예시: AX, AY, AZ, TEMP, GX, GY, GZ 각각 H, L 순서 */
    const uint8_t burst[14] = {
        0x00, 0x78,   /* AX =    120 */
        0xFF, 0xD8,   /* AY =    -40 */
        0x3F, 0xAC,   /* AZ =  16300 */
        0xFB, 0x50,   /* TEMP= -1200 */
        0x00, 0x55,   /* GX =     85 */
        0xFF, 0xF4,   /* GY =    -12 */
        0x00, 0x1E,   /* GZ =     30 */
    };
    const char *names[7] = { "AX", "AY", "AZ", "TEMP", "GX", "GY", "GZ" };
    for (int i = 0; i < 7; i++) {
        int16_t v = be16_to_i16(&burst[i * 2]);
        printf("%-4s H=0x%02X L=0x%02X -> %6d\n", names[i],
               (unsigned)burst[i * 2], (unsigned)burst[i * 2 + 1], v);
    }
    /* 온도 변환식(데이터시트): T(도) = raw / 340 + 36.53 -> x100 정수로 */
    int16_t traw = be16_to_i16(&burst[6]);
    long t_x100 = (long)traw * 100 / 340 + 3653;
    printf("temperature = %ld.%02ld C\n", t_x100 / 100, t_x100 % 100);

    printf("\n== 4. bit-field register model ==\n");
    GyroConfig gc;
    gc.reg = 0x00;
    gc.bits.fs_sel = 0;           /* ±250 dps : SPEC 의 설정값 */
    printf("FS_SEL=0 -> reg=0x%02X\n", (unsigned)gc.reg);
    gc.bits.fs_sel = 3;           /* ±2000 dps */
    printf("FS_SEL=3 -> reg=0x%02X\n", (unsigned)gc.reg);
    gc.bits.xg_st = 1;
    printf("XG_ST=1  -> reg=0x%02X\n", (unsigned)gc.reg);
    /* 같은 일을 마스크로 하면 (gcc 의 비트필드 배치와 결과가 같습니다) */
    uint8_t reg = 0x00;
    reg = (uint8_t)((reg & ~(3U << 3)) | (3U << 3));
    reg |= (uint8_t)(1U << 7);
    printf("mask way -> reg=0x%02X\n", (unsigned)reg);

    printf("\n== 5. bit-field flags ==\n");
    RobotFlags f = { 0 };
    f.motor_enable = 1;
    f.imu_ok = 1;
    f.state = 3;                  /* BALANCE */
    printf("sizeof(RobotFlags)=%u motor=%u low=%u imu=%u bt=%u state=%u\n",
           (unsigned)sizeof(RobotFlags), f.motor_enable, f.low_battery, f.imu_ok,
           f.bt_timeout, f.state);
    /* 4비트 필드에 17 을 넣으면 하위 4비트(1)만 남습니다. 상수 17 을 직접 대입하면
       gcc 가 -Woverflow 경고로 알려 줍니다. 실제로는 계산 결과가 들어오는 경우가
       많아 컴파일러가 잡지 못하므로, 여기서도 변수를 거쳐 대입합니다. */
    uint8_t new_state = 17;
    f.state = new_state;
    printf("state=17 stored as %u\n", f.state);

    return 0;
}

/*
 * [연습 문제]
 *  1. int16_t 값을 빅엔디언 2바이트로 쓰는 함수 void i16_to_be16(int16_t v, uint8_t *p) 를
 *     작성하고, be16_to_i16 으로 다시 읽어 원래 값이 나오는지 확인해 보세요.
 *  2. be16_to_i16 에서 (uint16_t) 캐스트 없이 (p[0] << 8) | p[1] 로 쓰면
 *     AVR(16비트 int) 에서 어떤 위험이 있는지 생각해 보세요.
 *  3. ACCEL_CONFIG(0x1C) 레지스터의 AFS_SEL(bit4..3)도 같은 모양입니다.
 *     AccelConfig 공용체를 만들고 ±8 g (AFS_SEL=2) 일 때 레지스터 값을 출력해 보세요.
 */
