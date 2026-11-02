/*
 * bsw_gpio.c
 * D3 PD3 버튼(입력, 풀업), D4 PD4 IN3, D7 PD7 IN1, D8 PB0 IN2, D9 PB1 IN4, D10 PB2 LED
 * PWM 핀(D5 PD5, D6 PD6)의 출력 설정은 bsw_pwm 이 담당합니다.
 */
#include <avr/io.h>
#include "bsw_gpio.h"

void bsw_gpio_init(void)
{
    DDRD  |= (1 << PD4) | (1 << PD7);
    DDRB  |= (1 << PB0) | (1 << PB1) | (1 << PB2);
    PORTD &= (uint8_t)~((1 << PD4) | (1 << PD7));
    PORTB &= (uint8_t)~((1 << PB0) | (1 << PB1) | (1 << PB2));

    DDRD  &= (uint8_t)~(1 << PD3);
    PORTD |= (1 << PD3);                   /* 내부 풀업 */
}

void bsw_gpio_led(uint8_t on)
{
    if (on) PORTB |= (1 << PB2);
    else    PORTB &= (uint8_t)~(1 << PB2);
}

void bsw_gpio_led_toggle(void)
{
    PINB = (1 << PB2);                     /* PIN 레지스터에 1 쓰기 = 토글 */
}

uint8_t bsw_gpio_button_pressed(void)
{
    return (PIND & (1 << PD3)) ? 0u : 1u;
}

void bsw_gpio_motor_l_dir(uint8_t in1, uint8_t in2)
{
    if (in1) PORTD |= (1 << PD7); else PORTD &= (uint8_t)~(1 << PD7);
    if (in2) PORTB |= (1 << PB0); else PORTB &= (uint8_t)~(1 << PB0);
}

void bsw_gpio_motor_r_dir(uint8_t in3, uint8_t in4)
{
    if (in3) PORTD |= (1 << PD4); else PORTD &= (uint8_t)~(1 << PD4);
    if (in4) PORTB |= (1 << PB1); else PORTB &= (uint8_t)~(1 << PB1);
}
