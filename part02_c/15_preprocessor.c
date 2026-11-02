/*
 * 15_preprocessor.c : 전처리기, 매크로 함수의 함정, 조건부 컴파일
 *
 * [학습 목표]
 *  1. #define 이 컴파일 전에 일어나는 "글자 치환" 이라는 것을 이해합니다.
 *  2. 매크로 함수에서 인자와 전체에 괄호가 필요한 이유를 출력으로 확인합니다.
 *  3. 인자가 두 번 평가되는 부작용을 이해하고, static inline 함수와 비교합니다.
 *  4. #if, #ifdef, #error 로 설정 매크로(기능 켜고 끄기, 값 검증)를 다룹니다.
 *  5. #, ## 연산자와 미리 정의된 매크로(__FILE__, __LINE__, __func__)를 써 봅니다.
 *
 * gcc -E 15_preprocessor.c 로 전처리 결과만 보면 치환 결과를 직접 확인할 수 있습니다.
 * gcc -DDEBUG_LOG=0 처럼 명령줄에서 매크로 값을 바꿀 수도 있습니다.
 */

#include <stdio.h>
#include <stdint.h>

/* ---------- 1. 설정 매크로 ----------
 * 펌웨어 전체의 설정값을 한곳에 모읍니다. 숫자 뒤의 UL 은 unsigned long 이라는 뜻으로,
 * AVR(16비트 int) 에서 16000000 이 넘치지 않게 해 줍니다. */
#ifndef F_CPU
#define F_CPU          16000000UL
#endif
#define BAUD           9600UL
#define CONTROL_HZ     200
#define MOTOR_L_INVERT 0
#define MOTOR_R_INVERT 1

#ifndef DEBUG_LOG
#define DEBUG_LOG      1        /* 1 이면 로그 출력, 0 이면 로그 코드가 통째로 사라짐 */
#endif

/* 컴파일 시점 검사: 조건이 맞지 않으면 빌드를 멈춥니다. */
#if (CONTROL_HZ < 50) || (CONTROL_HZ > 1000)
#error "CONTROL_HZ out of range"
#endif

/* UART 보레이트 레지스터 계산 (U2X0 = 1 일 때) : SPEC 의 UBRR0 = 207 과 같아야 합니다 */
#define UBRR_VALUE ((F_CPU / (8UL * BAUD)) - 1UL)

/* ---------- 2. 매크로 함수와 괄호 ---------- */
#define SQUARE_BAD(x)   x * x
#define SQUARE_GOOD(x)  ((x) * (x))
#define DOUBLE_BAD(x)   (x) + (x)
#define DOUBLE_GOOD(x)  ((x) + (x))

/* ---------- 3. 두 번 평가 문제 ---------- */
#define MAX_MACRO(a, b) (((a) > (b)) ? (a) : (b))
static inline int max_inline(int a, int b) { return (a > b) ? a : b; }

static int g_calls = 0;
static int next_value(void) { return ++g_calls * 10; }   /* 호출될 때마다 값이 바뀝니다 */

/* ---------- 4. 여러 문장 매크로는 do { } while (0) 로 ----------
 * 이렇게 감싸야 if (x) LOG2(...); else ... 에서도 문법이 깨지지 않습니다. */
#if DEBUG_LOG
#define LOG(msg) do { printf("[%s:%d] %s\n", __FILE__, __LINE__, (msg)); } while (0)
#else
#define LOG(msg) do { } while (0)
#endif

/* ---------- 5. # 과 ## ---------- */
#define STR(x)        #x                /* 인자를 문자열로 */
#define XSTR(x)       STR(x)            /* 매크로를 먼저 펼친 뒤 문자열로 */
#define REG_NAME(n)   PORT##n           /* 토큰 붙이기 */
static uint8_t PORTB = 0, PORTD = 0;

/* 모터 방향 반전 설정을 매크로로 적용 */
#if MOTOR_R_INVERT
#define MOTOR_R_SIGN (-1)
#else
#define MOTOR_R_SIGN (1)
#endif
#if MOTOR_L_INVERT
#define MOTOR_L_SIGN (-1)
#else
#define MOTOR_L_SIGN (1)
#endif

int main(void)
{
    printf("== 1. config macros ==\n");
    printf("F_CPU=%lu BAUD=%lu UBRR_VALUE=%lu\n",
           (unsigned long)F_CPU, (unsigned long)BAUD, (unsigned long)UBRR_VALUE);
    printf("control period = %d us\n", 1000000 / CONTROL_HZ);

    printf("\n== 2. parentheses matter ==\n");
    int a = 3;
    /* SQUARE_BAD(a + 1) -> a + 1 * a + 1 -> 3 + 3 + 1 = 7 */
    printf("SQUARE_BAD(a + 1)  = %d\n", SQUARE_BAD(a + 1));
    printf("SQUARE_GOOD(a + 1) = %d\n", SQUARE_GOOD(a + 1));
    /* 10 * DOUBLE_BAD(a) -> 10 * (a) + (a) = 33 */
    printf("10 * DOUBLE_BAD(a)  = %d\n", 10 * DOUBLE_BAD(a));
    printf("10 * DOUBLE_GOOD(a) = %d\n", 10 * DOUBLE_GOOD(a));

    printf("\n== 3. double evaluation ==\n");
    g_calls = 0;
    int m1 = MAX_MACRO(next_value(), 5);
    printf("MAX_MACRO(next_value(), 5) = %d, calls = %d\n", m1, g_calls);
    g_calls = 0;
    int m2 = max_inline(next_value(), 5);
    printf("max_inline(next_value(), 5) = %d, calls = %d\n", m2, g_calls);
    /* 매크로는 next_value() 를 두 번 호출해 엉뚱한 값(20)을 냅니다.
       인자에 ++ 나 함수 호출을 넣지 않거나, 처음부터 inline 함수를 씁니다. */

    printf("\n== 4. conditional compile and do-while(0) ==\n");
    if (a > 0)
        LOG("a is positive");
    else
        LOG("a is not positive");
#if DEBUG_LOG
    printf("DEBUG_LOG is ON (build with -DDEBUG_LOG=0 to remove logs)\n");
#else
    printf("DEBUG_LOG is OFF\n");
#endif

    printf("\n== 5. # and ## ==\n");
    printf("STR(CONTROL_HZ)  = %s\n", STR(CONTROL_HZ));
    printf("XSTR(CONTROL_HZ) = %s\n", XSTR(CONTROL_HZ));
    REG_NAME(B) = 0x04;     /* PORTB = 0x04; */
    REG_NAME(D) = 0x80;     /* PORTD = 0x80; */
    printf("PORTB=0x%02X PORTD=0x%02X\n", (unsigned)PORTB, (unsigned)PORTD);

    printf("\n== 6. motor invert config ==\n");
    int cmd = 120;
    printf("cmd=%d -> left=%d right=%d\n", cmd, cmd * MOTOR_L_SIGN, cmd * MOTOR_R_SIGN);

    printf("\n== 7. predefined macros ==\n");
    /* __FILE__ 은 gcc 에 넘긴 경로 그대로, __func__ 는 C99 의 현재 함수 이름입니다. */
    printf("file=%s line=%d func=%s\n", __FILE__, __LINE__, __func__);
#ifdef __AVR__
    printf("target: AVR\n");
#else
    printf("target: PC (not __AVR__)\n");
#endif

    return 0;
}

/*
 * [연습 문제]
 *  1. gcc -E 15_preprocessor.c | tail -n 80 으로 main 의 전처리 결과를 보고,
 *     SQUARE_BAD(a + 1) 이 어떻게 펼쳐졌는지 찾아보세요.
 *  2. 4장의 비트 매크로 BIT(n) 를 #define BIT(n) 1U << n 으로 바꾸면
 *     BIT(2) * 2 의 결과가 어떻게 되는지 예측하고 확인해 보세요.
 *  3. BAUD 를 115200UL 로 바꾸면 UBRR_VALUE 가 얼마가 되는지 계산하고,
 *     실제 보레이트 오차(%)를 구해 보세요. 오차가 2 % 를 넘으면 #error 를
 *     내도록 전처리 검사를 추가해 보세요.
 */
