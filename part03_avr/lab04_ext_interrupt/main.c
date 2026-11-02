/*
 * lab04_ext_interrupt: INT1 (D3 = PD3) 하강 엣지 외부 인터럽트
 *
 * 배선:
 *   버튼: D3 (PD3) 와 GND 사이 (내부 풀업)
 *   LED : D10 (PB2) 상태 LED, 버튼을 누를 때마다 토글
 *   D13 (PB5) 보드 L LED: 메인 루프 하트비트 (메인 루프가 다른 일을 하고 있다는 표시)
 *
 * 핵심:
 *   EICRA: ISC11=1, ISC10=0 -> INT1 하강 엣지 (High -> Low, 즉 버튼 누름 순간)
 *   EIMSK: INT1=1 -> INT1 인터럽트 허용
 *   EIFR : INTF1 에 1 을 써서 대기 중인 플래그 지우기
 *   sei(): SREG 의 I 비트를 켜서 전역 인터럽트 허용
 *
 * ISR 에서는 플래그만 세우고 실제 처리는 메인 루프에서 합니다.
 * ISR 과 메인이 함께 쓰는 변수는 반드시 volatile 로 선언합니다.
 */
#include <avr/io.h>
#include <avr/interrupt.h>
#include <util/delay.h>

#define BTN_BIT  PD3
#define LED_BIT  PB2
#define HB_BIT   PB5

static volatile uint8_t g_btn_flag;    /* ISR 이 1 로 세우고 메인이 0 으로 지움 */
static volatile uint8_t g_isr_count;   /* ISR 이 실제로 몇 번 불렸는지 (채터링 관찰용) */

ISR(INT1_vect)
{
    g_btn_flag = 1;
    g_isr_count++;
}

static void int1_init(void)
{
    DDRD  &= (uint8_t)~(1 << BTN_BIT);
    PORTD |= (1 << BTN_BIT);               /* 내부 풀업 */

    EICRA = (uint8_t)((EICRA & (uint8_t)~((1 << ISC11) | (1 << ISC10))) | (1 << ISC11)); /* 하강 엣지 */
    EIFR  = (1 << INTF1);                  /* 설정 중 생긴 플래그 지우기 (1 을 써야 지워짐) */
    EIMSK |= (1 << INT1);
}

int main(void)
{
    DDRB |= (1 << LED_BIT) | (1 << HB_BIT);
    int1_init();
    sei();

    uint8_t hb_div = 0;
    uint8_t handled = 0;                   /* 메인이 처리한 누름 횟수 */

    for (;;) {
        if (g_btn_flag) {
            g_btn_flag = 0;
            PINB = (1 << LED_BIT);         /* LED 토글 */
            handled++;

            /* 간단한 디바운스: 50 ms 동안 INT1 을 막고, 그동안 생긴 플래그를 버립니다 */
            EIMSK &= (uint8_t)~(1 << INT1);
            _delay_ms(50);
            EIFR  = (1 << INTF1);
            EIMSK |= (1 << INT1);
        }

        /* 메인 루프의 다른 일: 10 ms x 50 = 500 ms 마다 하트비트 */
        _delay_ms(10);
        if (++hb_div >= 50) {
            hb_div = 0;
            PINB = (1 << HB_BIT);
        }
    }
    (void)handled;
}
