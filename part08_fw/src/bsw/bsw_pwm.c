/*
 * bsw_pwm.c
 * Timer0 Fast PWM 8bit (WGM01:0 = 11), 분주 8
 *   f = 16 MHz / (8 * 256) = 7812.5 Hz
 * Fast PWM 에서 OCR0x = 0 이어도 매 주기 1 클럭 폭의 펄스가 나옵니다.
 * 완전 정지를 보장하려고 duty 0 일 때는 비교출력(COM0x1)을 끊고 핀을 LOW 로 둡니다.
 */
#include <avr/io.h>
#include "bsw_pwm.h"

void bsw_pwm_init(void)
{
    DDRD  |= (1 << PD5) | (1 << PD6);
    PORTD &= (uint8_t)~((1 << PD5) | (1 << PD6));
    OCR0A = 0;
    OCR0B = 0;
    TCCR0A = (1 << WGM01) | (1 << WGM00);   /* 비교출력은 duty > 0 일 때만 연결 */
    TCCR0B = (1 << CS01);                   /* clk/8 */
}

void bsw_pwm_set_a(uint8_t duty)
{
    if (duty == 0u) {
        TCCR0A &= (uint8_t)~(1 << COM0A1);
        PORTD  &= (uint8_t)~(1 << PD6);
    } else {
        OCR0A = duty;
        TCCR0A |= (1 << COM0A1);            /* 비반전 */
    }
}

void bsw_pwm_set_b(uint8_t duty)
{
    if (duty == 0u) {
        TCCR0A &= (uint8_t)~(1 << COM0B1);
        PORTD  &= (uint8_t)~(1 << PD5);
    } else {
        OCR0B = duty;
        TCCR0A |= (1 << COM0B1);
    }
}
