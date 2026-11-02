/*
 * bsw_uart.c
 * U2X0 = 1 (배속 모드): UBRR = F_CPU / (8 * baud) - 1 = 16e6 / 76800 - 1 = 207.3 -> 207
 * 실제 보레이트 = 16e6 / (8 * 208) = 9615 bps, 오차 +0.16 %
 *
 * 수신: RX 완료 인터럽트가 링버퍼에 저장, 메인 루프가 꺼내 씀
 * 송신: 링버퍼에 넣고 UDRE(데이터 레지스터 빔) 인터럽트가 한 바이트씩 내보냄
 *       -> 텔레메트리 송신이 제어 루프를 막지 않음
 * 버퍼 크기는 2의 거듭제곱이라 인덱스를 & (SIZE - 1) 로 감쌉니다.
 * 인덱스는 uint8_t 라 읽기/쓰기가 원자적이며, 각 인덱스는 한쪽(ISR 또는 메인)만 씁니다.
 */
#include <avr/io.h>
#include <avr/interrupt.h>
#include "bsw_uart.h"

#define RX_SIZE 64u
#define TX_SIZE 128u
#define RX_MASK (RX_SIZE - 1u)
#define TX_MASK (TX_SIZE - 1u)

static volatile char    s_rx[RX_SIZE];
static volatile uint8_t s_rx_head;     /* ISR 이 씀 */
static volatile uint8_t s_rx_tail;     /* 메인이 씀 */
static volatile uint8_t s_rx_ovf;

static volatile char    s_tx[TX_SIZE];
static volatile uint8_t s_tx_head;     /* 메인이 씀 */
static volatile uint8_t s_tx_tail;     /* ISR 이 씀 */

ISR(USART_RX_vect)
{
    char c = (char)UDR0;               /* 읽어야 플래그가 지워짐 */
    uint8_t next = (uint8_t)((s_rx_head + 1u) & RX_MASK);
    if (next != s_rx_tail) {
        s_rx[s_rx_head] = c;
        s_rx_head = next;
    } else {
        s_rx_ovf = 1;                  /* 가득 참: 새 바이트 버림 */
    }
}

ISR(USART_UDRE_vect)
{
    if (s_tx_tail != s_tx_head) {
        UDR0 = (uint8_t)s_tx[s_tx_tail];
        s_tx_tail = (uint8_t)((s_tx_tail + 1u) & TX_MASK);
    } else {
        UCSR0B &= (uint8_t)~(1 << UDRIE0);   /* 보낼 것 없음: 인터럽트 끔 */
    }
}

void bsw_uart_init(void)
{
    UCSR0A = (1 << U2X0);
    UBRR0  = 207;
    UCSR0C = (1 << UCSZ01) | (1 << UCSZ00);               /* 8N1 */
    UCSR0B = (1 << RXEN0) | (1 << TXEN0) | (1 << RXCIE0);
    s_rx_head = s_rx_tail = 0;
    s_tx_head = s_tx_tail = 0;
    s_rx_ovf = 0;
}

uint8_t bsw_uart_getc(char *c)
{
    if (s_rx_tail == s_rx_head) {
        return 0;
    }
    *c = s_rx[s_rx_tail];
    s_rx_tail = (uint8_t)((s_rx_tail + 1u) & RX_MASK);
    return 1;
}

uint8_t bsw_uart_tx_free(void)
{
    return (uint8_t)((s_tx_tail - s_tx_head - 1u) & TX_MASK);
}

uint8_t bsw_uart_write(const char *s, uint8_t len)
{
    /* 프레임이 잘려 나가지 않도록 전부 들어갈 때만 넣습니다 */
    if (len > bsw_uart_tx_free()) {
        return 0;
    }
    for (uint8_t i = 0; i < len; i++) {
        s_tx[s_tx_head] = s[i];
        s_tx_head = (uint8_t)((s_tx_head + 1u) & TX_MASK);
    }
    UCSR0B |= (1 << UDRIE0);
    return len;
}

uint8_t bsw_uart_rx_overflowed(void)
{
    uint8_t v = s_rx_ovf;
    s_rx_ovf = 0;
    return v;
}
