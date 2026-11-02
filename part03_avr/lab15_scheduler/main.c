/*
 * lab15_scheduler: 1 ms 틱 기반 협력형(cooperative) 스케줄러와 태스크 실행시간 측정
 *
 * 배선: USB 시리얼(9600 bps), 상태 LED D10 (PB2), 버튼 D3 (PD3, GND 로, 내부 풀업)
 *
 * 구조:
 *   Timer1 CTC 1 ms 인터럽트는 틱 카운터만 올립니다 (lab05).
 *   메인 루프가 틱이 바뀐 것을 보고 주기가 된 태스크를 차례로 호출합니다.
 *     task_1ms   : 버튼 샘플링과 디바운스        (1 ms 마다)
 *     task_10ms  : LED 패턴 상태 기계             (10 ms 마다)
 *     task_100ms : 상태 출력, 실행시간 통계 보고  (100 ms 마다, 출력은 1초마다)
 *   태스크는 끝까지 실행되고 스스로 돌려줘야 합니다 (선점 없음). 그래서 태스크 안에서
 *   _delay_ms() 나 while 대기 같은 블로킹 코드를 쓰면 안 됩니다.
 *
 * 실행시간 측정:
 *   타임스탬프 = 틱(ms) x 250 + TCNT1  (TCNT1 은 4 us 마다 1 증가, 0..249)
 *   태스크 앞뒤의 타임스탬프 차이 x 4 = 실행시간(us). 최댓값과 평균을 보고합니다.
 *   한 틱 안에 모든 태스크가 끝나지 않으면 다음 틱을 놓치는데, 이를 overrun 으로 셉니다.
 *
 * 9600 bps 에서 한 글자(10비트)는 약 1.04 ms 걸리고 이 실습의 송신은 폴링입니다.
 * 1초마다 보내는 보고는 약 140 글자이므로 task_100ms 한 번이 약 150 ms 동안 CPU 를 붙잡고,
 * 그동안 1 ms 태스크가 약 150 번 실행되지 못합니다. 보고에서 100ms 의 max 가 크고
 * overrun 이 초당 약 150 씩 늘어나는 것을 직접 확인합니다 (Part 8 에서는 송신을
 * 링버퍼와 UDRE 인터럽트로 바꿔 해결합니다).
 */
#include <avr/io.h>
#include <avr/interrupt.h>
#include <avr/pgmspace.h>
#include <util/atomic.h>
#include <stdio.h>

/* ================= 1 ms 틱 ================= */
static volatile uint32_t g_ms;
ISR(TIMER1_COMPA_vect) { g_ms++; }

static void timer1_init_1ms(void)
{
    TCCR1A = 0;
    TCCR1B = 0;
    TCNT1  = 0;
    OCR1A  = 249;
    TIFR1  = (1 << OCF1A);
    TIMSK1 = (1 << OCIE1A);
    TCCR1B = (1 << WGM12) | (1 << CS11) | (1 << CS10);   /* CTC, 분주 64 */
}

static uint32_t millis(void)
{
    uint32_t m;
    ATOMIC_BLOCK(ATOMIC_RESTORESTATE) { m = g_ms; }
    return m;
}

/* 4 us 단위 타임스탬프. 인터럽트를 막은 상태에서 TCNT1 이 0 으로 돌아갔지만 ISR 이 아직
 * 실행되지 않은 경우(OCF1A = 1, TCNT1 이 작음)를 보정합니다 */
static uint32_t ticks4us(void)
{
    uint32_t m;
    uint16_t t;
    ATOMIC_BLOCK(ATOMIC_RESTORESTATE) {
        m = g_ms;
        t = TCNT1;
        if ((TIFR1 & (1 << OCF1A)) && t < 125) m++;
    }
    return m * 250u + t;
}

/* ================= UART ================= */
static void uart_init(void)
{
    UBRR0  = 207;
    UCSR0A = (1 << U2X0);
    UCSR0B = (1 << TXEN0);
    UCSR0C = (1 << UCSZ01) | (1 << UCSZ00);
}
static int uart_stream_put(char c, FILE *stream)
{
    if (c == '\n') uart_stream_put('\r', stream);
    while (!(UCSR0A & (1 << UDRE0))) { }
    UDR0 = (uint8_t)c;
    return 0;
}
static FILE uart_out = FDEV_SETUP_STREAM(uart_stream_put, NULL, _FDEV_SETUP_WRITE);

/* ================= 태스크 실행시간 통계 ================= */
typedef struct {
    uint16_t max4;      /* 최대 실행시간 (4 us 단위) */
    uint32_t sum4;      /* 합계 (평균 계산용) */
    uint16_t count;     /* 실행 횟수 */
} task_stat_t;

enum { T_1MS, T_10MS, T_100MS, T_NUM };
static task_stat_t g_stat[T_NUM];
static uint16_t g_overrun;

static void stat_add(uint8_t id, uint32_t d4)
{
    task_stat_t *s = &g_stat[id];
    uint16_t d = (d4 > 0xFFFFu) ? 0xFFFFu : (uint16_t)d4;
    if (d > s->max4) s->max4 = d;
    s->sum4 += d;
    s->count++;
}

/* ================= 태스크 ================= */
static uint8_t g_btn_pressed;      /* task_1ms 가 쓰고 task_10ms 가 읽음 (같은 문맥이므로 volatile 불필요) */
static uint8_t g_mode;             /* 0 느린 깜박임, 1 빠른 깜박임 */

static void task_1ms(void)
{
    static uint8_t stable, cnt;
    uint8_t raw = (PIND & (1 << PD3)) == 0;
    if (raw != stable) {
        if (++cnt >= 20) {         /* 20 ms 디바운스 */
            stable = raw;
            cnt = 0;
            if (stable) g_btn_pressed = 1;
        }
    } else {
        cnt = 0;
    }
}

static void task_10ms(void)
{
    static uint8_t phase;
    if (g_btn_pressed) {
        g_btn_pressed = 0;
        g_mode ^= 1;
        phase = 0;
    }
    uint8_t period = g_mode ? 10 : 50;     /* x 10 ms: 100 ms 또는 500 ms 마다 토글 */
    if (++phase >= period) {
        phase = 0;
        PINB = (1 << PB2);
    }
}

static void task_100ms(void)
{
    static uint8_t div;
    if (++div < 10) return;                /* 1초에 한 번 보고 */
    div = 0;

    static const char name0[] PROGMEM = "1ms  ";
    static const char name1[] PROGMEM = "10ms ";
    static const char name2[] PROGMEM = "100ms";
    static const char *const names[T_NUM] PROGMEM = { name0, name1, name2 };

    printf_P(PSTR("t=%lu mode=%u overrun=%u\n"), millis(), g_mode, g_overrun);
    for (uint8_t i = 0; i < T_NUM; i++) {
        task_stat_t s;
        ATOMIC_BLOCK(ATOMIC_RESTORESTATE) { s = g_stat[i]; }
        uint32_t avg_us = s.count ? (s.sum4 * 4u) / s.count : 0;
        printf_P(PSTR(" %S n=%u avg=%lu us max=%lu us\n"),
                 (const char *)pgm_read_word(&names[i]), s.count, avg_us, (uint32_t)s.max4 * 4u);
        g_stat[i].max4 = 0;
        g_stat[i].sum4 = 0;
        g_stat[i].count = 0;
    }
    /* 이 보고 자체가 약 150 ms 걸리므로 다음 보고의 100ms 태스크 max 와 overrun 에 그대로 나타납니다 */
}

/* 측정 래퍼 */
static void run_task(uint8_t id, void (*fn)(void))
{
    uint32_t t0 = ticks4us();
    fn();
    stat_add(id, ticks4us() - t0);
}

int main(void)
{
    DDRB  |= (1 << PB2);
    DDRD  &= (uint8_t)~(1 << PD3);
    PORTD |= (1 << PD3);
    uart_init();
    stdout = &uart_out;
    timer1_init_1ms();
    sei();

    printf_P(PSTR("\nlab15 scheduler\n"));

    uint32_t last = millis();
    uint8_t  c10 = 0, c100 = 0;

    for (;;) {
        uint32_t now = millis();
        if (now == last) {
            continue;                      /* 다음 틱까지 대기 (여기서 sleep 모드를 쓸 수도 있습니다) */
        }
        uint32_t elapsed = now - last;
        if (elapsed > 1) g_overrun += (uint16_t)(elapsed - 1);   /* 놓친 틱 수 */
        last = now;

        /* 놓친 틱을 몰아서 실행하지 않고, 이번 틱 한 번만 처리합니다 */
        run_task(T_1MS, task_1ms);
        if (++c10 >= 10) {
            c10 = 0;
            run_task(T_10MS, task_10ms);
            if (++c100 >= 10) {
                c100 = 0;
                run_task(T_100MS, task_100ms);
            }
        }
    }
}
