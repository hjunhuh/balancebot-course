/*
 * lab10_uart_rx_ringbuf: RX 완료 인터럽트 + 링버퍼 + 줄 단위 명령 파서
 *
 * 배선:
 *   USB 케이블 (PC 터미널 9600 bps, 8N1, 줄 끝 "CR+LF" 또는 "LF")
 *   상태 LED D10 (PB2): "LED ON" / "LED OFF"
 *   D6 (PD6, OC0A) -> 330 옴 -> LED -> GND: "PWM 0..255" 로 밝기 (lab06 과 같은 배선)
 *
 * 명령 (대소문자 구분, 한 줄에 하나):
 *   LED ON      상태 LED 켜기          응답: OK
 *   LED OFF     상태 LED 끄기          응답: OK
 *   PWM n       D6 밝기 n (0..255)     응답: OK PWM=n
 *   그 밖       응답: ERR ...
 *
 * 구조:
 *   [UART RX 하드웨어] -> ISR(USART_RX_vect) -> 링버퍼(64 B) -> 메인 루프 -> 줄 버퍼 -> 파서
 *   ISR 은 1바이트를 링버퍼에 넣기만 하고 바로 끝납니다. 9600 bps 에서 1바이트는 약 1.04 ms 이므로
 *   메인 루프가 잠깐 바빠도 링버퍼가 64바이트(약 67 ms)까지 받아 둡니다.
 *
 * 링버퍼 규칙 (단일 생산자 ISR, 단일 소비자 메인):
 *   head 는 ISR 만 쓰고, tail 은 메인만 씁니다. 인덱스가 uint8_t 라 읽기/쓰기가 원자적입니다.
 *   head == tail 이면 비어 있음, (head + 1) % SIZE == tail 이면 가득 참 (한 칸은 비워 둠).
 *   SIZE 를 2의 거듭제곱으로 하면 % 대신 & (SIZE - 1) 로 빠르게 계산합니다.
 */
#include <avr/io.h>
#include <avr/interrupt.h>
#include <avr/pgmspace.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#define RX_BUF_SIZE  64u                          /* 2의 거듭제곱 */
#define RX_BUF_MASK  (RX_BUF_SIZE - 1u)
#define LINE_MAX     32u

static volatile uint8_t rx_buf[RX_BUF_SIZE];
static volatile uint8_t rx_head;                  /* ISR 이 다음에 쓸 위치 */
static volatile uint8_t rx_tail;                  /* 메인이 다음에 읽을 위치 */
static volatile uint8_t rx_overflow;              /* 버퍼가 가득 차 버린 바이트 수 */

ISR(USART_RX_vect)
{
    uint8_t status = UCSR0A;                      /* 오류 비트는 UDR0 을 읽기 전에 읽어야 합니다 */
    uint8_t c = UDR0;                             /* UDR0 을 읽어야 RXC0 플래그가 지워집니다 */
    if (status & ((1 << FE0) | (1 << DOR0))) {
        return;                                   /* 프레임 오류, 오버런 바이트는 버림 */
    }
    uint8_t next = (uint8_t)((rx_head + 1u) & RX_BUF_MASK);
    if (next != rx_tail) {
        rx_buf[rx_head] = c;
        rx_head = next;
    } else {
        rx_overflow++;
    }
}

/* 링버퍼에서 1바이트 꺼내기. 없으면 -1 */
static int16_t rx_get(void)
{
    if (rx_head == rx_tail) return -1;
    uint8_t c = rx_buf[rx_tail];
    rx_tail = (uint8_t)((rx_tail + 1u) & RX_BUF_MASK);
    return c;
}

static void uart_init(void)
{
    UBRR0  = 207;                                 /* 9600 bps @ U2X */
    UCSR0A = (1 << U2X0);
    UCSR0C = (1 << UCSZ01) | (1 << UCSZ00);       /* 8N1 */
    UCSR0B = (1 << RXEN0) | (1 << TXEN0) | (1 << RXCIE0);   /* 수신, 송신, RX 완료 인터럽트 */
}

static void uart_putc(char c)
{
    while (!(UCSR0A & (1 << UDRE0))) { }
    UDR0 = (uint8_t)c;
}

static int uart_stream_put(char c, FILE *stream)
{
    (void)stream;
    if (c == '\n') uart_putc('\r');
    uart_putc(c);
    return 0;
}
static FILE uart_out = FDEV_SETUP_STREAM(uart_stream_put, NULL, _FDEV_SETUP_WRITE);

/* ---- 출력 장치 ---- */
static void led_init(void)
{
    DDRB |= (1 << PB2);
}

static void pwm_init(void)
{
    DDRD  |= (1 << PD6);
    PORTD &= (uint8_t)~(1 << PD6);
    TCCR0A = (1 << WGM01) | (1 << WGM00);         /* Fast PWM 8비트 */
    TCCR0B = (1 << CS01);                         /* 분주 8, 7812.5 Hz */
}

static void pwm_set(uint8_t duty)
{
    if (duty == 0) {
        TCCR0A &= (uint8_t)~((1 << COM0A1) | (1 << COM0A0));
    } else {
        OCR0A   = duty;
        TCCR0A |= (1 << COM0A1);
    }
}

/* ---- 명령 파서 ---- */

/* 문자열 전체가 0..max 범위 10진수이면 1, 결과는 *out */
static uint8_t parse_uint(const char *s, uint16_t max, uint16_t *out)
{
    if (*s == '\0') return 0;
    uint16_t v = 0;
    while (*s) {
        if (*s < '0' || *s > '9') return 0;
        v = (uint16_t)(v * 10u + (uint16_t)(*s - '0'));
        if (v > max) return 0;                    /* 넘치기 전에 바로 거절 */
        s++;
    }
    *out = v;
    return 1;
}

static void handle_line(char *line)
{
    if (line[0] == '\0') return;                  /* 빈 줄 무시 */

    if (strcmp_P(line, PSTR("LED ON")) == 0) {
        PORTB |= (1 << PB2);
        printf_P(PSTR("OK\n"));
    } else if (strcmp_P(line, PSTR("LED OFF")) == 0) {
        PORTB &= (uint8_t)~(1 << PB2);
        printf_P(PSTR("OK\n"));
    } else if (strncmp_P(line, PSTR("PWM "), 4) == 0) {
        uint16_t v;
        if (parse_uint(line + 4, 255, &v)) {
            pwm_set((uint8_t)v);
            printf_P(PSTR("OK PWM=%u\n"), v);
        } else {
            printf_P(PSTR("ERR range 0..255\n"));
        }
    } else {
        printf_P(PSTR("ERR unknown: %s\n"), line);
    }
}

int main(void)
{
    led_init();
    pwm_init();
    uart_init();
    stdout = &uart_out;
    sei();

    printf_P(PSTR("\nlab10 ready. LED ON | LED OFF | PWM n\n"));

    char line[LINE_MAX + 1];
    uint8_t len = 0;
    uint8_t too_long = 0;

    for (;;) {
        int16_t r = rx_get();
        if (r < 0) {
            /* 받은 것이 없으면 다른 일을 할 수 있습니다 (여기서는 없음) */
            continue;
        }
        char c = (char)r;

        if (c == '\r') continue;                  /* CR 무시, LF 로 줄 끝 판단 */
        if (c == '\n') {
            line[len] = '\0';
            if (too_long) printf_P(PSTR("ERR too long\n"));
            else          handle_line(line);
            len = 0;
            too_long = 0;
            continue;
        }
        if (len < LINE_MAX) line[len++] = c;
        else                too_long = 1;         /* 줄 끝까지 버리고 오류 1번만 보고 */

        if (rx_overflow) {
            rx_overflow = 0;
            printf_P(PSTR("WARN rx overflow\n"));
        }
    }
}
