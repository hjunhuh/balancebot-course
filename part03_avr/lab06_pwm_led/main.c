/*
 * lab06_pwm_led: Timer0 Fast PWM 으로 LED 밝기 조절 (숨쉬는 LED)
 *
 * 하드웨어 PWM 은 타이머의 출력 비교 핀(OCnx)에서만 나옵니다.
 * Timer0 의 출력 핀은 OC0A = PD6 (D6), OC0B = PD5 (D5) 입니다.
 * 상태 LED 가 있는 D10 (PB2) 은 OC1B (Timer1) 이므로 Timer0 PWM 이 나오지 않습니다.
 * 그래서 이 실습에서는 D6 에 LED 를 따로 꽂아 실습합니다.
 *
 * 배선: D6 (PD6, OC0A) -> 330 옴 -> LED -> GND
 *       (로봇에서 D6 은 L298N ENA 입니다. 이 실습 중에는 L298N 을 빼 두세요)
 *
 * Timer0 Fast PWM 8비트 (WGM02:0 = 011, TOP = 0xFF), 분주 8:
 *   PWM 주파수 = F_CPU / (N x 256) = 16000000 / (8 x 256) = 7812.5 Hz
 *   비반전(COM0A1:0 = 10): 듀티 = (OCR0A + 1) / 256
 *   OCR0A = 0 이어도 한 클럭(1/256) 폭의 펄스가 나가므로, 완전히 끄려면
 *   COM0A 를 끊고(일반 포트로 되돌리고) PORT 를 0 으로 둡니다. -> pwm_a_set()
 */
#include <avr/io.h>
#include <util/delay.h>

static void timer0_pwm_init(void)
{
    DDRD  |= (1 << PD6);                          /* OC0A 핀을 출력으로 (필수) */
    PORTD &= (uint8_t)~(1 << PD6);
    OCR0A  = 0;
    TCCR0A = (1 << WGM01) | (1 << WGM00);         /* Fast PWM 8비트, COM0A 는 pwm_a_set 에서 */
    TCCR0B = (1 << CS01);                         /* WGM02 = 0, 분주 8, 타이머 시작 */
}

/* duty: 0 = 완전히 꺼짐, 1..255 = (duty + 1) / 256 */
static void pwm_a_set(uint8_t duty)
{
    if (duty == 0) {
        TCCR0A &= (uint8_t)~((1 << COM0A1) | (1 << COM0A0));   /* PWM 출력 끊기 -> PORTD6 = 0 */
    } else {
        OCR0A   = duty;
        TCCR0A |= (1 << COM0A1);                                /* 비반전 */
    }
}

/*
 * 사람 눈은 밝기를 로그적으로 느끼므로 선형으로 듀티를 올리면
 * 처음에 급히 밝아지고 위쪽에서는 변화가 거의 안 보입니다.
 * 제곱 곡선(감마 약 2)으로 보정합니다: out = in^2 / 255
 */
static uint8_t gamma2(uint8_t in)
{
    return (uint8_t)(((uint16_t)in * in + 254) / 255);
}

int main(void)
{
    timer0_pwm_init();

    for (;;) {
        /* 1단계: 감마 보정한 숨쉬기 */
        for (uint16_t i = 0; i <= 255; i++) {
            pwm_a_set(gamma2((uint8_t)i));
            _delay_ms(4);
        }
        for (int16_t i = 255; i >= 0; i--) {
            pwm_a_set(gamma2((uint8_t)i));
            _delay_ms(4);
        }
        _delay_ms(300);

        /* 2단계: 고정 단계 0, 25, 50, 75, 100 % 를 1초씩 (선형) */
        static const uint8_t steps[] = { 0, 63, 127, 191, 255 };
        for (uint8_t k = 0; k < sizeof(steps); k++) {
            pwm_a_set(steps[k]);
            _delay_ms(1000);
        }
        pwm_a_set(0);
        _delay_ms(500);
    }
}
