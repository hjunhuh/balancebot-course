/*
 * lab03_gpio_in: 버튼 읽기, 내부 풀업, 소프트웨어 디바운스
 *
 * 배선:
 *   버튼: D3 (PD3) 와 GND 사이에 푸시버튼 (외부 저항 없음, 내부 풀업 사용)
 *   LED : D10 (PB2) -> 330 옴 -> LED -> GND  (디바운스된 누름마다 토글)
 *   D13 (PB5) 보드 L LED: 버튼의 원시(raw) 상태를 그대로 표시
 *
 * 내부 풀업: DDRD3 = 0 (입력), PORTD3 = 1 이면 약 20~50 k옴 풀업이 켜집니다.
 * 따라서 버튼을 안 누르면 PIND3 = 1, 누르면 GND 로 연결되어 PIND3 = 0 (액티브 로우)입니다.
 * MCUCR 의 PUD 비트가 1 이면 모든 풀업이 꺼지므로 0(리셋값)인지 확인합니다.
 */
#include <avr/io.h>
#include <util/delay.h>

#define BTN_BIT        PD3
#define LED_BIT        PB2
#define RAW_LED_BIT    PB5

#define USE_DEBOUNCE   1     /* 0 으로 바꿔 빌드하면 채터링으로 LED 가 여러 번 토글되는 것을 볼 수 있습니다 */
#define DEBOUNCE_MS    20    /* 이 시간 동안 같은 값이 유지되어야 상태 변경으로 인정 */

static uint8_t btn_raw_pressed(void)
{
    return (PIND & (1 << BTN_BIT)) == 0;   /* 액티브 로우 */
}

int main(void)
{
    DDRB |= (1 << LED_BIT) | (1 << RAW_LED_BIT);

    DDRD  &= (uint8_t)~(1 << BTN_BIT);     /* 입력 */
    PORTD |= (1 << BTN_BIT);               /* 내부 풀업 켜기 */
    _delay_us(10);                         /* 풀업이 핀을 충전하고 입력 동기화기를 통과할 시간 */

    uint8_t stable  = btn_raw_pressed();   /* 디바운스된 현재 상태 */
    uint8_t counter = 0;                   /* 다른 값이 연속으로 관찰된 횟수(ms) */
    uint16_t presses = 0;                  /* 누른 횟수 (lab09 이후 UART 로 출력해 볼 수 있습니다) */

    for (;;) {
        uint8_t raw = btn_raw_pressed();

        /* 원시 상태를 D13 에 그대로 표시 */
        if (raw) PORTB |= (1 << RAW_LED_BIT);
        else     PORTB &= (uint8_t)~(1 << RAW_LED_BIT);

#if USE_DEBOUNCE
        /* 1 ms 마다 샘플링하여 DEBOUNCE_MS 번 연속 다른 값이면 상태 변경 */
        if (raw != stable) {
            if (++counter >= DEBOUNCE_MS) {
                stable = raw;
                counter = 0;
                if (stable) {                 /* 눌림 엣지(뗌 -> 누름)에서만 동작 */
                    PINB = (1 << LED_BIT);    /* LED 토글 */
                    presses++;
                }
            }
        } else {
            counter = 0;                      /* 튀는 값이 나오면 다시 처음부터 */
        }
#else
        /* 디바운스 없음: 엣지를 바로 인식. 채터링마다 토글됩니다 */
        if (raw && !stable) {
            PINB = (1 << LED_BIT);
            presses++;
        }
        stable = raw;
#endif
        _delay_ms(1);
    }
    (void)presses;
}
