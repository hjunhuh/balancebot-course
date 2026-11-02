/*
 * lab08_adc: ADC0 단일 변환으로 배터리 전압 측정 (정수 연산)
 *
 * 배선:
 *   배터리(+) -> 20 k옴 -> A0 (PC0, ADC0) -> 10 k옴 -> GND,  A0 와 GND 사이 100 nF
 *   배터리(-) 와 Uno GND 공통
 *   상태 LED D10 (PB2): 저전압이면 빠르게 깜박임
 *   결과 출력: UART 9600 bps (USB 시리얼 모니터). UART 자체는 lab09 에서 자세히 다룹니다.
 *
 * 분압: Vadc = Vbat x 10k / (20k + 10k) = Vbat / 3
 *   8.4 V(만충) -> 2.80 V, 7.4 V(공칭) -> 2.47 V 로 AVcc(5 V) 기준 범위 안에 들어옵니다.
 *
 * ADC 설정:
 *   ADMUX : REFS1:0 = 01 (AVcc 기준, AREF 핀에 콘덴서), ADLAR = 0 (오른쪽 정렬), MUX3:0 = 0000 (ADC0)
 *   ADCSRA: ADEN = 1, ADPS2:0 = 111 (분주 128 -> 16 MHz / 128 = 125 kHz, 권장 50~200 kHz)
 *   DIDR0 : ADC0D = 1 (디지털 입력 버퍼 끄기, 소비 전류와 잡음 감소)
 *
 * 환산 (정수만 사용):
 *   Vadc[mV] = ADC x VREF_MV / 1024
 *   Vbat[mV] = Vadc x 3 = ADC x (VREF_MV x 3) / 1024
 *   VREF_MV = 5000 이면 ADC x 15000 / 1024, 1 LSB = 약 14.6 mV
 *   ADC 최댓값 1023 x 15000 = 15,345,000 은 uint16_t 를 넘으므로 uint32_t 로 계산합니다.
 */
#include <avr/io.h>
#include <util/delay.h>

#define VREF_MV        5000UL   /* 실제 5V 핀 전압을 멀티미터로 재서 넣으면 정확도가 올라갑니다 */
#define DIV_NUM        3UL      /* (20k + 10k) / 10k */
#define LOW_BATT_MV    6800     /* 예시 임계값 (2S, 셀당 3.4 V). 실제 값은 Part 8 에서 정합니다 */

/* ---- 결과 출력용 최소 UART (9600 bps, U2X, 8N1) ---- */
static void uart_init(void)
{
    UBRR0  = 207;
    UCSR0A = (1 << U2X0);
    UCSR0B = (1 << TXEN0);
    UCSR0C = (1 << UCSZ01) | (1 << UCSZ00);
}
static void uart_putc(char c)
{
    while (!(UCSR0A & (1 << UDRE0))) { }
    UDR0 = (uint8_t)c;
}
static void uart_puts(const char *s)
{
    while (*s) uart_putc(*s++);
}
static void uart_putu(uint16_t v)          /* 부호 없는 10진수 출력 */
{
    char buf[6];
    uint8_t i = 0;
    do { buf[i++] = (char)('0' + v % 10); v /= 10; } while (v);
    while (i) uart_putc(buf[--i]);
}

/* ---- ADC ---- */
static void adc_init(void)
{
    ADMUX  = (1 << REFS0);                                   /* AVcc, ADC0 */
    ADCSRA = (1 << ADEN) | (1 << ADPS2) | (1 << ADPS1) | (1 << ADPS0);
    DIDR0  = (1 << ADC0D);
    /* 첫 변환은 아날로그 회로 초기화 때문에 25 클럭이 걸리고 값도 버립니다 */
    ADCSRA |= (1 << ADSC);
    while (ADCSRA & (1 << ADSC)) { }
}

static uint16_t adc_read(uint8_t ch)
{
    ADMUX = (uint8_t)((ADMUX & 0xF0) | (ch & 0x0F));         /* 기준 전압 비트는 유지 */
    ADCSRA |= (1 << ADSC);                                   /* 단일 변환 시작 */
    while (ADCSRA & (1 << ADSC)) { }                         /* 끝나면 ADSC 가 0 으로 돌아옴 (13 클럭 = 104 us) */
    return ADC;                                              /* ADCL 을 먼저, ADCH 를 나중에 읽는 16비트 접근 */
}

/* 16회 평균: 잡음을 줄이고 결과는 여전히 0..1023 */
static uint16_t adc_read_avg16(uint8_t ch)
{
    uint16_t sum = 0;                                        /* 1023 x 16 = 16368, uint16_t 로 충분 */
    for (uint8_t i = 0; i < 16; i++) sum += adc_read(ch);
    return (uint16_t)((sum + 8) >> 4);
}

static uint16_t adc_to_vbat_mv(uint16_t raw)
{
    return (uint16_t)(((uint32_t)raw * (VREF_MV * DIV_NUM) + 512) >> 10);   /* /1024, 반올림 */
}

int main(void)
{
    DDRB |= (1 << PB2);
    uart_init();
    adc_init();
    uart_puts("lab08 ADC battery\r\n");

    for (;;) {
        uint16_t raw = adc_read_avg16(0);
        uint16_t mv  = adc_to_vbat_mv(raw);

        uart_puts("ADC=");
        uart_putu(raw);
        uart_puts(" VBAT=");
        uart_putu(mv);
        uart_puts(" mV");
        if (mv < LOW_BATT_MV) uart_puts(" LOW");
        uart_puts("\r\n");

        if (mv < LOW_BATT_MV) {
            for (uint8_t i = 0; i < 5; i++) { PINB = (1 << PB2); _delay_ms(100); }
            PORTB &= (uint8_t)~(1 << PB2);
        } else {
            PORTB |= (1 << PB2);
            _delay_ms(500);
        }
    }
}
