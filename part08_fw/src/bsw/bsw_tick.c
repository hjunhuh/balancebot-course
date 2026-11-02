/*
 * bsw_tick.c
 * Timer1 CTC 모드, 분주 64: 16 MHz / 64 = 250 kHz
 * OCR1A = 250 - 1 = 249 -> 1 kHz 비교일치 인터럽트 = 1 ms
 */
#include <avr/io.h>
#include <avr/interrupt.h>
#include <util/atomic.h>
#include "bsw_tick.h"

static volatile uint32_t s_ms;

ISR(TIMER1_COMPA_vect)
{
    s_ms++;
}

void bsw_tick_init(void)
{
    TCCR1A = 0;
    TCCR1B = (1 << WGM12) | (1 << CS11) | (1 << CS10);   /* CTC(OCR1A), clk/64 */
    OCR1A  = 249;
    TCNT1  = 0;
    TIMSK1 = (1 << OCIE1A);
}

uint32_t bsw_tick_ms(void)
{
    uint32_t v;
    ATOMIC_BLOCK(ATOMIC_RESTORESTATE) {   /* 32비트 값은 4번에 나눠 읽으므로 보호 */
        v = s_ms;
    }
    return v;
}
