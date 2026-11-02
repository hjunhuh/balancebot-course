/*
 * 16_volatile_static_const.c : 스코프, static, const, volatile
 *
 * [학습 목표]
 *  1. 블록 스코프, 파일 스코프, 그리고 변수의 수명(lifetime)을 구분합니다.
 *  2. static 지역 변수(값 유지)와 static 전역 변수/함수(파일 안에 숨김)를 구분합니다.
 *  3. const 로 바뀌지 않는 값을 표시하고, 포인터와 함께 쓸 때의 위치별 의미를 압니다.
 *  4. volatile 이 "이 값은 컴파일러가 모르게 바뀔 수 있다" 는 뜻임을
 *     ISR(인터럽트) 과 메인 루프가 변수를 공유하는 예로 이해합니다.
 *  5. 공유 변수의 원자성(atomic) 문제와 인터럽트 금지 구간을 개념적으로 압니다.
 */

#include <stdio.h>
#include <stdint.h>

/* ---------- 1. 파일 스코프 ----------
 * 함수 밖에 선언한 변수는 프로그램 시작부터 끝까지 살아 있습니다 (정적 수명).
 * static 을 붙이면 이 .c 파일 안에서만 보입니다 (다른 파일에서 extern 으로 못 씀).
 * 이름 충돌을 막기 위해 모듈 내부 변수는 항상 static 으로 둡니다. */
static int g_module_counter = 0;

/* static 함수도 이 파일 안에서만 호출할 수 있습니다. */
static void bump(void) { g_module_counter++; }

/* ---------- 2. static 지역 변수 ----------
 * 함수가 끝나도 값이 유지되고, 초기화는 프로그램 시작 시 한 번만 됩니다.
 * 스택이 아니라 전역 변수와 같은 영역(.data / .bss)에 놓입니다. */
static int call_count_static(void)
{
    static int count = 0;   /* 한 번만 0 으로 초기화 */
    count++;
    return count;
}

static int call_count_auto(void)
{
    int count = 0;          /* 호출될 때마다 새로 만들어지고 0 으로 초기화 */
    count++;
    return count;
}

/* 깜빡임처럼 "이전 상태" 를 기억해야 하는 함수에 static 지역 변수가 잘 맞습니다. */
static uint8_t led_toggle(void)
{
    static uint8_t led = 0;
    led ^= 1U;
    return led;
}

/* ---------- 3. const ---------- */
static const uint16_t LOW_BATT_MV = 6400;     /* 바꿀 수 없는 상수 */

/* 표는 const 로 두면 AVR 에서 컴파일러가 최적화하기 좋고, 실수로 덮어쓰지 않습니다.
   (AVR 에서 RAM 을 아끼려면 PROGMEM 으로 플래시에 두는 방법을 Part 3 에서 봅니다.) */
static const int16_t SIN_TABLE_X1000[5] = { 0, 259, 500, 707, 866 };  /* 0,15,30,45,60 도 */

static void const_pointer_demo(void)
{
    int16_t x = 1, y = 2;

    const int16_t *p1 = &x;     /* 가리키는 값을 못 바꿈, 포인터는 바꿀 수 있음 */
    /* *p1 = 5;  오류 */
    p1 = &y;

    int16_t *const p2 = &x;     /* 포인터를 못 바꿈, 값은 바꿀 수 있음 */
    *p2 = 5;
    /* p2 = &y;  오류 */

    const int16_t *const p3 = &y;  /* 둘 다 못 바꿈 */

    printf("*p1=%d x=%d *p3=%d\n", *p1, x, *p3);
}

/* ---------- 4. volatile ----------
 * 실제 AVR 코드에서는 다음과 같은 모양입니다.
 *
 *   volatile uint8_t  g_tick_flag = 0;   // ISR 이 1 로, main 이 0 으로
 *   volatile uint32_t g_millis = 0;      // ISR 이 1 ms 마다 증가
 *
 *   ISR(TIMER1_COMPA_vect)               // 1 ms 마다 하드웨어가 호출
 *   {
 *       g_millis++;
 *       if (++sub >= 5) { sub = 0; g_tick_flag = 1; }
 *   }
 *
 *   int main(void) {
 *       ...
 *       for (;;) {
 *           while (!g_tick_flag) { }     // 플래그를 기다림
 *           g_tick_flag = 0;
 *           control_step();
 *       }
 *   }
 *
 * volatile 이 없으면 컴파일러(-Os)는 "이 루프 안에서 g_tick_flag 를 바꾸는 코드가
 * 없다" 고 판단해 값을 레지스터에 한 번만 읽어 두고 다시 읽지 않습니다.
 * 그러면 ISR 이 1 로 바꿔도 main 은 영원히 기다립니다.
 * volatile 은 "읽고 쓸 때마다 반드시 실제 메모리에 접근하라" 는 지시입니다.
 * 하드웨어 레지스터(PORTB, UDR0 등)도 같은 이유로 volatile 로 정의되어 있습니다.
 *
 * 주의: volatile 은 원자성을 보장하지 않습니다. AVR 은 8비트 CPU 라서
 * uint32_t g_millis 를 읽는 데 명령이 여러 개 필요하고, 그 사이에 ISR 이 끼어들면
 * 반쯤 바뀐 값을 읽게 됩니다. 그래서 다음처럼 인터럽트를 잠깐 막고 복사합니다.
 *
 *   uint32_t millis(void) {
 *       uint32_t m;
 *       uint8_t sreg = SREG; cli();      // 인터럽트 금지
 *       m = g_millis;
 *       SREG = sreg;                     // 이전 상태 복원
 *       return m;
 *   }
 *
 * PC 에서는 ISR 이 없으므로, 아래에서 "fake_timer_isr" 함수를 직접 호출해
 * 흐름만 흉내 냅니다.
 */
static volatile uint8_t  g_tick_flag = 0;
static volatile uint32_t g_millis = 0;

static void fake_timer_isr(void)
{
    static uint8_t sub = 0;    /* ISR 안의 static 지역 변수: 호출 사이에 값 유지 */
    g_millis++;
    if (++sub >= 5) {
        sub = 0;
        g_tick_flag = 1;
    }
}

/* 32비트 값이 바이트 단위로 나뉘어 갱신되는 도중에 읽으면 어떤 값이 보이는지 흉내 냅니다. */
static void torn_read_demo(void)
{
    union { uint32_t v; uint8_t b[4]; } counter = { .v = 0x000000FFu };
    /* 0x000000FF + 1 = 0x00000100. 8비트 CPU 는 하위 바이트부터 차례로 씁니다.
       하위 바이트만 쓴 순간(0x00)에 ISR 이 끼어들어 읽으면 0x00000000 이 보입니다. */
    uint32_t next = counter.v + 1;
    counter.b[0] = (uint8_t)(next & 0xFF);          /* 1단계: 하위 바이트 갱신 */
    uint32_t seen_by_isr = counter.v;               /* 이 순간 끼어든 ISR 이 본 값 */
    counter.b[1] = (uint8_t)((next >> 8) & 0xFF);   /* 2단계: 다음 바이트 갱신 */
    printf("expected 0x%08lX, ISR saw 0x%08lX in the middle, final 0x%08lX\n",
           (unsigned long)next, (unsigned long)seen_by_isr, (unsigned long)counter.v);
}

int main(void)
{
    printf("== 1. block scope and shadowing ==\n");
    int v = 1;
    {
        int v = 2;              /* 바깥 v 를 가립니다 (shadowing). 헷갈리니 피합니다. */
        printf("inner v = %d\n", v);
    }
    printf("outer v = %d\n", v);
    for (int i = 0; i < 1; i++) { /* i 는 이 for 안에서만 존재 */ }

    printf("\n== 2. static local vs auto local ==\n");
    for (int i = 0; i < 3; i++) {
        int s = call_count_static();
        int a = call_count_auto();
        printf("call %d: static=%d auto=%d\n", i + 1, s, a);
    }
    printf("led: %u %u %u %u\n", led_toggle(), led_toggle(), led_toggle(), led_toggle());
    /* 주의: 함수 인자의 평가 순서는 정해져 있지 않습니다. 위 줄은 순서에 따라
       출력이 다를 수 있는 나쁜 예입니다. 아래처럼 한 번에 하나씩 부르는 것이 맞습니다. */
    printf("led in order:");
    for (int i = 0; i < 4; i++) printf(" %u", led_toggle());
    printf("\n");

    printf("\n== 3. static global (file scope) ==\n");
    bump(); bump(); bump();
    printf("g_module_counter = %d\n", g_module_counter);

    printf("\n== 4. const ==\n");
    printf("LOW_BATT_MV = %u\n", (unsigned)LOW_BATT_MV);
    printf("sin(30 deg) x1000 = %d\n", SIN_TABLE_X1000[2]);
    const_pointer_demo();

    printf("\n== 5. volatile flag between ISR and main (simulated) ==\n");
    int control_runs = 0;
    for (int ms = 0; ms < 20; ms++) {
        fake_timer_isr();               /* 실제로는 하드웨어가 1 ms 마다 호출 */
        if (g_tick_flag) {              /* main 루프의 확인 */
            g_tick_flag = 0;
            control_runs++;
            printf("millis=%2lu -> control_step #%d\n", (unsigned long)g_millis, control_runs);
        }
    }

    printf("\n== 6. why atomic access matters (simulated torn read) ==\n");
    torn_read_demo();

    return 0;
}

/*
 * [연습 문제]
 *  1. call_count_static 과 같은 동작을 static 전역 변수로 바꿔 구현해 보고,
 *     둘 중 어느 쪽이 더 안전한지(다른 함수가 건드릴 수 있는지) 설명해 보세요.
 *  2. 다음 중 volatile 이 필요한 변수를 고르세요:
 *     (가) ISR 에서 증가시키는 수신 바이트 수  (나) main 에서만 쓰는 PID 적분값
 *     (다) UART 데이터 레지스터 UDR0          (라) const 룩업 테이블
 *  3. uint8_t 인 g_tick_flag 는 cli() 없이 읽어도 되지만 uint32_t g_millis 는
 *     안 되는 이유를 8비트 CPU 의 명령 수 관점에서 설명해 보세요.
 */
