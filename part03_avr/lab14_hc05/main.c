/*
 * lab14_hc05: HC-05 블루투스 모듈, 데이터 모드에서 받은 줄을 되돌려 보내는 에코
 *
 * 배선 (SPEC 핀맵):
 *   HC-05 VCC -> 5V,  GND -> GND
 *   HC-05 TXD -> D0 (PD0, RXD0)
 *   HC-05 RXD <- D1 (PD1, TXD0) 를 1k/2k 분압으로 (D1 - 1k - RXD - 2k - GND, 5 V -> 3.3 V)
 *   상태 LED D10 (PB2): 한 줄을 받을 때마다 토글
 *
 * 주의: D0/D1 은 Uno 의 USB 시리얼(16U2)과 같은 핀입니다.
 *   HC-05 와 USB 시리얼 모니터를 동시에 쓰면 두 송신기가 D0 에서 충돌합니다.
 *   이 실습 중에는 PC 시리얼 모니터를 닫고, 전원만 USB 로 공급합니다.
 *   (ISP 로 기록하므로 HC-05 를 꽂은 채로 기록해도 됩니다. USB 부트로더 업로드라면 빼야 합니다)
 *
 * 데이터 모드: 9600 bps, 8N1 (HC-05 출고 기본값). 휴대폰과 페어링(PIN 1234 또는 0000) 후
 *   "Serial Bluetooth Terminal" 같은 앱에서 장치를 연결하고 줄 끝을 "CR+LF" 또는 "LF" 로 설정합니다.
 *
 * 동작: 받은 한 줄(최대 48바이트, SPEC 프레임 최대 길이와 동일)을 "ECHO: <줄>" 로 되돌려 보내고,
 *   5초마다 "#HB,<초>" 하트비트를 보냅니다.
 */
#include <avr/io.h>
#include <avr/interrupt.h>
#include <avr/pgmspace.h>
#include <util/atomic.h>
#include <stdio.h>

#define RX_BUF_SIZE  64u
#define RX_BUF_MASK  (RX_BUF_SIZE - 1u)
#define LINE_MAX     48u

/* ================= UART + 링버퍼 (lab10 과 동일 구조) ================= */
static volatile uint8_t rx_buf[RX_BUF_SIZE];
static volatile uint8_t rx_head, rx_tail;

ISR(USART_RX_vect)
{
    uint8_t st = UCSR0A;
    uint8_t c = UDR0;
    if (st & ((1 << FE0) | (1 << DOR0))) return;
    uint8_t next = (uint8_t)((rx_head + 1u) & RX_BUF_MASK);
    if (next != rx_tail) {
        rx_buf[rx_head] = c;
        rx_head = next;
    }
}

static int16_t rx_get(void)
{
    if (rx_head == rx_tail) return -1;
    uint8_t c = rx_buf[rx_tail];
    rx_tail = (uint8_t)((rx_tail + 1u) & RX_BUF_MASK);
    return c;
}

static void uart_init(void)
{
    UBRR0  = 207;                                   /* 9600 bps @ U2X, HC-05 데이터 모드 기본값 */
    UCSR0A = (1 << U2X0);
    UCSR0C = (1 << UCSZ01) | (1 << UCSZ00);
    UCSR0B = (1 << RXEN0) | (1 << TXEN0) | (1 << RXCIE0);
}

static int uart_stream_put(char c, FILE *stream)
{
    if (c == '\n') uart_stream_put('\r', stream);
    while (!(UCSR0A & (1 << UDRE0))) { }
    UDR0 = (uint8_t)c;
    return 0;
}
static FILE bt_out = FDEV_SETUP_STREAM(uart_stream_put, NULL, _FDEV_SETUP_WRITE);

/* ================= 1 ms 틱 (lab05 와 동일) ================= */
static volatile uint32_t g_ms;
ISR(TIMER1_COMPA_vect) { g_ms++; }

static void timer1_init_1ms(void)
{
    TCCR1A = 0;
    OCR1A  = 249;
    TIMSK1 = (1 << OCIE1A);
    TCCR1B = (1 << WGM12) | (1 << CS11) | (1 << CS10);
}

static uint32_t millis(void)
{
    uint32_t m;
    ATOMIC_BLOCK(ATOMIC_RESTORESTATE) { m = g_ms; }
    return m;
}

int main(void)
{
    DDRB |= (1 << PB2);
    uart_init();
    timer1_init_1ms();
    stdout = &bt_out;
    sei();

    printf_P(PSTR("\nlab14 HC-05 echo ready\n"));

    char line[LINE_MAX + 1];
    uint8_t len = 0;
    uint32_t t_hb = 0;

    for (;;) {
        int16_t r = rx_get();
        if (r >= 0) {
            char c = (char)r;
            if (c == '\r') {
                /* 무시 */
            } else if (c == '\n') {
                line[len] = '\0';
                printf_P(PSTR("ECHO: %s\n"), line);
                PINB = (1 << PB2);
                len = 0;
            } else if (len < LINE_MAX) {
                line[len++] = c;
            }
        }

        uint32_t now = millis();
        if ((uint32_t)(now - t_hb) >= 5000) {
            t_hb += 5000;
            printf_P(PSTR("#HB,%lu\n"), now / 1000);
        }
    }
}
