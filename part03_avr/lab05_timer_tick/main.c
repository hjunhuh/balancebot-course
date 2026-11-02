/*
 * lab05_timer_tick: Timer1 CTC 1 ms 틱, millis() 구현, ATOMIC_BLOCK
 *
 * 배선: D10 (PB2) 상태 LED, D13 (PB5) 보드 L LED
 *
 * Timer1 CTC (모드 4, TOP = OCR1A), 분주 64:
 *   타이머 클럭 = 16 MHz / 64 = 250 kHz (4 us 마다 1 증가)
 *   인터럽트 주파수 = F_CPU / (N x (1 + OCR1A)) = 16000000 / (64 x 250) = 1000 Hz
 *   따라서 OCR1A = 249
 *
 * g_ms 는 32비트이므로 8비트 AVR 은 4번에 나눠 읽습니다. 읽는 도중 ISR 이 값을 바꾸면
 * 앞뒤가 섞인 잘못된 값이 나오므로 ATOMIC_BLOCK 으로 인터럽트를 잠시 막고 읽습니다.
 */
#include <avr/io.h>
#include <avr/interrupt.h>
#include <util/atomic.h>

#define LED_BIT  PB2
#define L_BIT    PB5

static volatile uint32_t g_ms;   /* 부팅 후 경과 ms, 약 49.7일 후 0 으로 돌아감 */

ISR(TIMER1_COMPA_vect)
{
    g_ms++;
}

static void timer1_init_1ms(void)
{
    TCCR1A = 0;                                  /* OC1A/OC1B 핀 사용 안 함, WGM11:10 = 00 */
    TCCR1B = 0;                                  /* 설정 중에는 정지 */
    TCNT1  = 0;
    OCR1A  = 249;                                /* 250 카운트 = 1 ms */
    TIFR1  = (1 << OCF1A);                       /* 대기 플래그 지우기 */
    TIMSK1 = (1 << OCIE1A);                      /* 비교 일치 A 인터럽트 허용 */
    TCCR1B = (1 << WGM12) | (1 << CS11) | (1 << CS10);  /* CTC(WGM13:10=0100), 분주 64, 시작 */
}

uint32_t millis(void)
{
    uint32_t m;
    ATOMIC_BLOCK(ATOMIC_RESTORESTATE) {          /* cli, 읽기, 이전 SREG 복원 */
        m = g_ms;
    }
    return m;
}

int main(void)
{
    DDRB |= (1 << LED_BIT) | (1 << L_BIT);
    timer1_init_1ms();
    sei();

    uint32_t t_led = 0;
    uint32_t t_l   = 0;

    for (;;) {
        uint32_t now = millis();

        /* 블로킹 지연 없이 서로 다른 주기로 두 LED 를 깜박입니다.
         * (now - t) 형태의 부호 없는 뺄셈은 g_ms 가 0 으로 돌아가도 올바르게 동작합니다. */
        if ((uint32_t)(now - t_led) >= 500) {
            t_led += 500;
            PINB = (1 << LED_BIT);
        }
        if ((uint32_t)(now - t_l) >= 125) {
            t_l += 125;
            PINB = (1 << L_BIT);
        }
        /* 여기에 다른 일을 넣어도 LED 주기는 흔들리지 않습니다 */
    }
}
