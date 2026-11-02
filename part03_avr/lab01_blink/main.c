/*
 * lab01_blink: 상태 LED(D10 = PB2) 깜박이기
 *
 * 배선: D10(PB2) -> 330 옴 -> LED 애노드(+), LED 캐소드(-) -> GND
 * 빌드: avr-gcc -mmcu=atmega328p -DF_CPU=16000000UL -Os main.c
 *
 * 데이터시트 읽는 순서 (ATmega328P 데이터시트, "I/O-Ports" 장):
 *   1) Pin Configurations: PB2 가 몇 번 핀이고 어떤 대체 기능(SS, OC1B)이 있는지 확인합니다.
 *   2) I/O-Ports > Ports as General Digital I/O: DDxn, PORTxn, PINxn 세 비트의 역할을 읽습니다.
 *        DDxn  = 1 이면 출력, 0 이면 입력
 *        PORTxn: 출력일 때 1 은 High, 0 은 Low (입력일 때는 풀업 on/off)
 *        PINxn : 핀의 실제 논리 값 (읽기 전용, 1 을 쓰면 PORTxn 이 반전됨)
 *   3) Register Description: DDRB, PORTB, PINB 의 주소와 초깃값(모두 0x00)을 확인합니다.
 *        리셋 직후 모든 핀은 입력(하이 임피던스) 상태입니다.
 *   4) Electrical Characteristics: 핀당 최대 전류(절대 최대 40 mA, 권장 20 mA)를 확인하고
 *        직렬 저항을 고릅니다. (5 V - 2 V) / 330 옴 = 약 9 mA 로 안전합니다.
 */
#include <avr/io.h>
#include <util/delay.h>   /* _delay_ms(): F_CPU 와 최적화(-Os)가 있어야 정확합니다 */

#define LED_DDR   DDRB
#define LED_PORT  PORTB
#define LED_PIN   PINB
#define LED_BIT   PB2      /* PB2 는 avr/io.h 에서 2 로 정의되어 있습니다 */

int main(void)
{
    /* 1. 방향 설정: DDRB 의 2번 비트만 1 로 만들어 PB2 를 출력으로 */
    LED_DDR |= (1 << LED_BIT);

    /* 2. 초기 상태: Low (LED 꺼짐) */
    LED_PORT &= (uint8_t)~(1 << LED_BIT);

    for (;;) {
        /* 방법 A: PORT 비트를 직접 켜고 끄기 */
        LED_PORT |= (1 << LED_BIT);              /* 켜기 */
        _delay_ms(500);
        LED_PORT &= (uint8_t)~(1 << LED_BIT);    /* 끄기 */
        _delay_ms(500);

        /* 방법 B: PIN 레지스터에 1 을 쓰면 해당 PORT 비트가 반전됩니다 (한 명령, sbi) */
        LED_PIN = (1 << LED_BIT);
        _delay_ms(100);
        LED_PIN = (1 << LED_BIT);
        _delay_ms(100);
    }
}
