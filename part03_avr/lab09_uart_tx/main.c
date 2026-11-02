/*
 * lab09_uart_tx: UART0 송신, 9600 bps (U2X), printf 를 FILE 스트림으로 연결
 *
 * 배선: 추가 배선 없음. Uno 의 USB 케이블(16U2 USB 시리얼)로 PC 에서 확인합니다.
 *   ISP 로 기록한 뒤에도 USB 케이블은 전원과 시리얼 용도로 그대로 꽂아 둡니다.
 *   PC 터미널: 9600 bps, 8N1 (Arduino IDE 시리얼 모니터, PuTTY, Tera Term 등)
 *
 * 보율 계산 (U2X0 = 1, 2배속 모드):
 *   UBRR0 = F_CPU / (8 x BAUD) - 1 = 16000000 / 76800 - 1 = 207.33 -> 207
 *   실제 보율 = F_CPU / (8 x (UBRR0 + 1)) = 16000000 / 1664 = 9615.38 bps
 *   오차 = 9615.38 / 9600 - 1 = +0.16 %   (허용 범위 대략 +-2 % 이내)
 *
 * printf 연결: avr-libc 는 stdout 이 가리키는 FILE 의 put 함수로 한 글자씩 보냅니다.
 *   FDEV_SETUP_STREAM 은 정적 초기화이므로 malloc 을 쓰지 않습니다 (fdevopen 은 malloc 사용).
 *   기본 printf 는 %f 를 지원하지 않습니다. 실수는 100배 정수로 바꿔 출력합니다.
 */
#include <avr/io.h>
#include <avr/pgmspace.h>
#include <stdio.h>
#include <util/delay.h>

#define BAUD_UBRR  207

static void uart_init(void)
{
    UBRR0H = (uint8_t)(BAUD_UBRR >> 8);           /* UBRR0H 를 먼저 쓰고 UBRR0L 을 쓰면 갱신 */
    UBRR0L = (uint8_t)BAUD_UBRR;
    UCSR0A = (1 << U2X0);                         /* 2배속 */
    UCSR0B = (1 << TXEN0);                        /* 송신기만 켬 */
    UCSR0C = (1 << UCSZ01) | (1 << UCSZ00);       /* 비동기, 패리티 없음, 정지 1비트, 8비트 */
}

static void uart_putc(char c)
{
    while (!(UCSR0A & (1 << UDRE0))) { }          /* 송신 버퍼가 빌 때까지 대기 */
    UDR0 = (uint8_t)c;
}

static void uart_puts(const char *s)
{
    while (*s) uart_putc(*s++);
}

/* 문자열을 Flash 에서 직접 읽어 보냄: SRAM(2 KB)을 아낍니다 */
static void uart_puts_P(const char *s)
{
    char c;
    while ((c = (char)pgm_read_byte(s++)) != 0) uart_putc(c);
}

/* FILE 스트림의 put 함수: '\n' 앞에 '\r' 을 붙여 터미널 줄바꿈을 맞춥니다 */
static int uart_stream_put(char c, FILE *stream)
{
    (void)stream;
    if (c == '\n') uart_putc('\r');
    uart_putc(c);
    return 0;
}

static FILE uart_out = FDEV_SETUP_STREAM(uart_stream_put, NULL, _FDEV_SETUP_WRITE);

int main(void)
{
    uart_init();
    stdout = &uart_out;
    stderr = &uart_out;

    uart_puts("\r\nlab09 UART TX\r\n");
    uart_puts_P(PSTR("This string lives in Flash (PSTR)\r\n"));

    uint16_t count = 0;
    int16_t  angle_x100 = -1234;               /* -12.34 도를 100배 정수로 */

    for (;;) {
        /* printf: 정수, 16진수, 부호 */
        printf("count=%u hex=0x%04X\n", count, count);

        /* printf_P: 형식 문자열을 Flash 에 둡니다 */
        printf_P(PSTR("long=%ld\n"), (int32_t)count * 1000L);

        /* 100배 정수를 소수점 형태로 출력 (부호 처리 주의) */
        int16_t a = angle_x100;
        char sign = (a < 0) ? '-' : '+';
        if (a < 0) a = (int16_t)-a;
        printf("angle=%c%d.%02d deg\n", sign, a / 100, a % 100);

        count++;
        angle_x100 += 37;
        _delay_ms(1000);
    }
}
