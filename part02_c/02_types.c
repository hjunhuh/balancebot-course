/*
 * 02_types.c : 자료형, 고정폭 정수, sizeof, 오버플로
 *
 * [학습 목표]
 *  1. char, short, int, long, float, double 의 크기가 플랫폼마다 다를 수 있음을
 *     sizeof 로 확인합니다.
 *  2. stdint.h 의 고정폭 정수(uint8_t, int16_t, uint32_t 등)를 왜 임베디드에서
 *     기본으로 쓰는지 설명할 수 있습니다.
 *  3. 부호 없는 정수의 순환(wrap around)과 작은 자료형으로 바꿀 때 값이 잘리는
 *     현상을 직접 관찰합니다.
 *  4. 정수 나눗셈과 실수 나눗셈의 차이, 형 변환(cast)을 익힙니다.
 */

#include <stdio.h>
#include <stdint.h>    /* uint8_t, int16_t, uint32_t ... */
#include <inttypes.h>  /* PRIu32 같은 printf 형식 매크로 */

/*
 * 같은 int 라도 크기가 다릅니다.
 *   PC (x86-64, gcc) : int = 4바이트 (32비트)
 *   AVR (avr-gcc)    : int = 2바이트 (16비트), double 도 기본 4바이트
 * 그래서 "int 는 32비트겠지" 라고 가정한 코드를 AVR 로 옮기면 값이 넘칩니다.
 * 크기가 중요한 곳(레지스터, 센서 데이터, 통신 프레임)은 반드시 고정폭 정수를 씁니다.
 *
 *   uint8_t  : 0 .. 255               (레지스터 1개, 바이트 버퍼)
 *   int8_t   : -128 .. 127
 *   uint16_t : 0 .. 65535             (타이머 카운터, ADC 결과)
 *   int16_t  : -32768 .. 32767        (MPU6050 원시값)
 *   uint32_t : 0 .. 4294967295        (밀리초 카운터)
 *   int32_t  : 약 -21억 .. 21억       (곱셈 중간 결과)
 */

int main(void)
{
    printf("== 1. sizeof (bytes on this PC) ==\n");
    printf("char      : %u\n", (unsigned)sizeof(char));
    printf("short     : %u\n", (unsigned)sizeof(short));
    printf("int       : %u\n", (unsigned)sizeof(int));
    printf("long      : %u\n", (unsigned)sizeof(long));
    printf("float     : %u\n", (unsigned)sizeof(float));
    printf("double    : %u\n", (unsigned)sizeof(double));
    printf("uint8_t   : %u\n", (unsigned)sizeof(uint8_t));
    printf("int16_t   : %u\n", (unsigned)sizeof(int16_t));
    printf("uint32_t  : %u\n", (unsigned)sizeof(uint32_t));

    printf("\n== 2. unsigned wrap around ==\n");
    /* 부호 없는 정수는 최댓값에서 1을 더하면 0으로 돌아옵니다.
       C 표준이 보장하는 동작입니다 (모듈러 산술). */
    uint8_t u8 = 250;
    for (int i = 0; i < 8; i++) {
        printf("%u ", (unsigned)u8);
        u8++;
    }
    printf("\n");

    uint8_t zero = 0;
    zero--;   /* 0 - 1 = 255 */
    printf("uint8_t 0 - 1 = %u\n", (unsigned)zero);

    printf("\n== 3. truncation when storing into a smaller type ==\n");
    /* 300 은 uint8_t 에 들어가지 않으므로 하위 8비트(300 % 256 = 44)만 남습니다. */
    int big = 300;
    uint8_t cut = (uint8_t)big;
    printf("(uint8_t)300 = %u\n", (unsigned)cut);

    /* 부호 있는 작은 자료형으로 바꾸면 음수가 될 수 있습니다.
       (구현 정의 동작이지만 gcc 와 avr-gcc 는 2의 보수로 잘라 냅니다) */
    int raw = 40000;
    int16_t s16 = (int16_t)raw;
    printf("(int16_t)40000 = %d\n", s16);

    printf("\n== 4. 16-bit int trap (AVR style) ==\n");
    /* AVR 에서 int 는 16비트입니다. 여기서는 int16_t 로 그 상황을 흉내 냅니다.
       1000 ms * 100 = 100000 은 16비트 범위를 넘습니다. */
    int16_t ms = 1000;
    int16_t bad = (int16_t)(ms * 100);          /* 결과를 16비트에 담으면 깨집니다 */
    int32_t good = (int32_t)ms * 100;           /* 먼저 32비트로 넓힌 뒤 곱합니다 */
    printf("int16 result : %d\n", bad);
    printf("int32 result : %" PRId32 "\n", good);

    printf("\n== 5. integer vs floating division ==\n");
    int a = 7, b = 2;
    printf("7 / 2         = %d\n", a / b);              /* 정수 나눗셈: 소수점 버림 */
    printf("7 %% 2         = %d\n", a % b);             /* 나머지 */
    printf("7 / 2.0       = %.2f\n", a / 2.0);          /* 한쪽이 실수면 실수 연산 */
    printf("(float)7 / 2  = %.2f\n", (float)a / b);

    printf("\n== 6. fixed point instead of float ==\n");
    /* AVR 에는 부동소수점 하드웨어가 없어 float 연산이 느립니다.
       그래서 프로토콜 v1 처럼 "실수는 100배 정수" 로 주고받습니다. */
    int32_t angle_x100 = -1234;   /* -12.34 도 */
    printf("angle_x100 = %" PRId32 " -> %" PRId32 ".%02" PRId32 " deg\n",
           angle_x100,
           angle_x100 / 100,
           (angle_x100 < 0 ? -angle_x100 : angle_x100) % 100);

    printf("\n== 7. printf formats for fixed width ==\n");
    uint32_t millis = 4000000000u;
    printf("millis = %" PRIu32 "\n", millis);
    printf("0x%02X 0x%04X\n", 0xABu, 0x1234u);

    return 0;
}

/*
 * [연습 문제]
 *  1. int8_t 변수에 127 을 넣고 1 을 더해 int8_t 로 다시 저장하면 어떤 값이
 *     출력되는지 예측하고 확인해 보세요.
 *  2. ADC 값(0..1023)을 전압(mV, 기준 5000 mV)으로 바꾸는 식을 uint16_t 만으로
 *     쓰면 어떤 문제가 생기는지 설명하고, uint32_t 를 써서 고쳐 보세요.
 *  3. 위 6번의 각도 출력은 -0.50 도처럼 정수부가 0인 음수에서 부호가 사라집니다.
 *     부호를 따로 처리해 올바르게 출력하도록 고쳐 보세요.
 */
