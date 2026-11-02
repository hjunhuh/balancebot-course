/*
 * lab02_gpio_out: 비트 연산으로 여러 LED 패턴 만들기, 상태 LED 블링크 코드
 *
 * 배선 (각 LED 는 330 옴 직렬, 캐소드는 GND):
 *   LED0: D8  (PB0)   주의: 로봇에서는 L298N IN2 핀, 이 실습 중에는 L298N 을 빼 두세요
 *   LED1: D9  (PB1)   주의: 로봇에서는 L298N IN4 핀
 *   LED2: D10 (PB2)   상태 LED (LED_STAT)
 *   LED3: D13 (PB5)   Uno 보드에 이미 있는 L LED, 배선 불필요
 *
 * LED 4개가 PORTB 의 연속되지 않은 비트(0, 1, 2, 5)에 있으므로
 * 4비트 값을 실제 핀 비트로 옮기는 함수 leds_write() 를 만들어 씁니다.
 */
#include <avr/io.h>
#include <util/delay.h>

#define LED0  PB0
#define LED1  PB1
#define LED2  PB2
#define LED3  PB5
#define LED_MASK  ((1 << LED0) | (1 << LED1) | (1 << LED2) | (1 << LED3))
#define STAT_BIT  LED2

/* _delay_ms() 의 인자는 컴파일 시간 상수여야 하므로, 변수 지연은 1 ms 반복으로 만듭니다 */
static void delay_ms_var(uint16_t ms)
{
    while (ms--) {
        _delay_ms(1);
    }
}

/* v 의 비트 0..3 을 LED0..LED3 에 출력. 다른 PORTB 비트는 건드리지 않습니다 */
static void leds_write(uint8_t v)
{
    uint8_t out = 0;
    if (v & 0x01) out |= (1 << LED0);
    if (v & 0x02) out |= (1 << LED1);
    if (v & 0x04) out |= (1 << LED2);
    if (v & 0x08) out |= (1 << LED3);
    /* 읽기, 수정, 쓰기: 마스크 밖 비트는 보존, 마스크 안 비트만 교체 */
    PORTB = (uint8_t)((PORTB & (uint8_t)~LED_MASK) | out);
}

/* 패턴 1: 하나씩 옮겨 가는 불빛 (왼쪽 시프트) */
static void pattern_running(void)
{
    for (uint8_t i = 0; i < 4; i++) {
        leds_write((uint8_t)(1 << i));
        _delay_ms(150);
    }
}

/* 패턴 2: 왕복 (나이트 라이더) */
static void pattern_bounce(void)
{
    static const uint8_t seq[] = { 0x1, 0x2, 0x4, 0x8, 0x4, 0x2 };
    for (uint8_t r = 0; r < 2; r++) {
        for (uint8_t i = 0; i < sizeof(seq); i++) {
            leds_write(seq[i]);
            _delay_ms(100);
        }
    }
}

/* 패턴 3: 0..15 이진 카운터 */
static void pattern_binary(void)
{
    for (uint8_t n = 0; n < 16; n++) {
        leds_write(n);
        _delay_ms(200);
    }
}

/* 패턴 4: 짝수, 홀수 LED 를 XOR 로 번갈아 반전 */
static void pattern_xor(void)
{
    uint8_t v = 0x5;                /* 0101 */
    for (uint8_t i = 0; i < 6; i++) {
        leds_write(v);
        v ^= 0xF;                   /* 모든 비트 반전: 0101 <-> 1010 */
        _delay_ms(200);
    }
}

/*
 * 상태 LED 블링크 코드: 상태 번호 n 만큼 짧게 깜박이고 길게 쉽니다.
 * Part 8 에서 로봇 상태(0 INIT, 1 CALIB, 3 BALANCE 등)를 눈으로 확인할 때 같은 방식을 씁니다.
 * 상태 번호 0 은 아주 짧게 한 번 깜박여 "살아 있음"만 표시합니다.
 */
static void status_blink(uint8_t n)
{
    if (n == 0) {
        PORTB |= (1 << STAT_BIT);
        _delay_ms(30);
        PORTB &= (uint8_t)~(1 << STAT_BIT);
    } else {
        for (uint8_t i = 0; i < n; i++) {
            PORTB |= (1 << STAT_BIT);
            _delay_ms(150);
            PORTB &= (uint8_t)~(1 << STAT_BIT);
            _delay_ms(250);
        }
    }
    delay_ms_var(1000);
}

int main(void)
{
    DDRB |= LED_MASK;      /* 4개 핀을 한 번에 출력으로 */
    leds_write(0);

    for (;;) {
        pattern_running();
        pattern_bounce();
        pattern_binary();
        pattern_xor();

        leds_write(0);
        status_blink(3);   /* 예: 상태 3 (BALANCE) 표시 */
    }
}
