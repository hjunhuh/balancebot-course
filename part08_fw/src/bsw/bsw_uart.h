/* bsw_uart.h : USART0 9600 bps 8N1, 인터럽트 + 링버퍼 송수신 */
#ifndef BSW_UART_H
#define BSW_UART_H
#include <stdint.h>

void    bsw_uart_init(void);
uint8_t bsw_uart_getc(char *c);                       /* 1: 받은 문자 있음 */
uint8_t bsw_uart_write(const char *s, uint8_t len);   /* 반환: 큐에 넣은 바이트 수 */
uint8_t bsw_uart_tx_free(void);                       /* 송신 버퍼 남은 칸 */
uint8_t bsw_uart_rx_overflowed(void);                 /* 수신 버퍼 넘침 발생 여부 (읽으면 지워짐) */

#endif
