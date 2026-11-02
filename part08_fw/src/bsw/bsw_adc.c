/*
 * bsw_adc.c
 * ADC 클럭은 50~200 kHz 권장: 16 MHz / 128 = 125 kHz
 * 한 번 변환 = 13 ADC 클럭 = 약 104 us (100 ms 주기 작업에서만 호출)
 */
#include <avr/io.h>
#include "bsw_adc.h"

void bsw_adc_init(void)
{
    ADMUX  = (1 << REFS0);                                         /* AVcc 기준 */
    ADCSRA = (1 << ADEN) | (1 << ADPS2) | (1 << ADPS1) | (1 << ADPS0);
    DIDR0  = (1 << ADC0D);                                         /* A0 디지털 입력 끔 */
}

uint16_t bsw_adc_read(uint8_t ch)
{
    ADMUX = (uint8_t)((ADMUX & 0xF0u) | (ch & 0x0Fu));
    ADCSRA |= (1 << ADSC);
    while (ADCSRA & (1 << ADSC)) {
        /* 변환 완료 대기 (최대 약 104 us) */
    }
    return ADC;                                                    /* ADCL 먼저 읽는 순서를 컴파일러가 지킴 */
}
