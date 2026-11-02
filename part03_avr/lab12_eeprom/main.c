/*
 * lab12_eeprom: EEAR/EEDR/EECR 레지스터로 EEPROM 직접 읽기/쓰기, 부팅 횟수 카운터
 *
 * 배선: USB 시리얼(9600 bps), 상태 LED D10 (PB2), 버튼 D3 (PD3, GND 로, 내부 풀업)
 *   부팅할 때 버튼을 누르고 있으면 카운터를 0 으로 초기화합니다.
 *   Uno 의 RESET 버튼을 누를 때마다 카운터가 1씩 늘어납니다.
 *
 * EEPROM: 1024 바이트, 주소 0x000..0x3FF, 셀당 약 100,000 회 쓰기 수명, 쓰기 1회 약 3.4 ms
 *
 * 쓰기 순서 (데이터시트 "EEPROM Data Memory" 의 순서 그대로):
 *   1. EECR 의 EEPE 가 0 이 될 때까지 대기 (이전 쓰기 완료)
 *   2. EEAR 에 주소, EEDR 에 데이터
 *   3. EECR 의 EEMPE 에 1 을 쓰면서 EEPE 는 0
 *   4. 4 클럭 안에 EEPE 에 1  -> 그 사이 인터럽트가 끼어들면 실패하므로 cli
 * 읽기: EEPE 대기, EEAR 에 주소, EERE 에 1, 다음 명령에서 EEDR 을 읽음
 *
 * 저장 형식 (주소):
 *   0x000      매직 바이트 0xA5 (지워진 EEPROM 은 0xFF 이므로 "처음 부팅" 판별용)
 *   0x001..2   부팅 횟수 uint16_t (리틀엔디언)
 */
#include <avr/io.h>
#include <avr/interrupt.h>
#include <avr/pgmspace.h>
#include <util/delay.h>
#include <stdio.h>

#define EE_ADDR_MAGIC  0x000
#define EE_ADDR_BOOTS  0x001
#define EE_MAGIC       0xA5

/* ================= UART ================= */
static void uart_init(void)
{
    UBRR0  = 207;
    UCSR0A = (1 << U2X0);
    UCSR0B = (1 << TXEN0);
    UCSR0C = (1 << UCSZ01) | (1 << UCSZ00);
}
static int uart_stream_put(char c, FILE *stream)
{
    if (c == '\n') uart_stream_put('\r', stream);
    while (!(UCSR0A & (1 << UDRE0))) { }
    UDR0 = (uint8_t)c;
    return 0;
}
static FILE uart_out = FDEV_SETUP_STREAM(uart_stream_put, NULL, _FDEV_SETUP_WRITE);

/* ================= EEPROM (레지스터 직접) ================= */
static uint8_t ee_read_byte(uint16_t addr)
{
    while (EECR & (1 << EEPE)) { }        /* 쓰기 진행 중이면 대기 */
    EEAR = addr;
    EECR |= (1 << EERE);                  /* 읽기 시작: CPU 는 4 클럭 멈춘 뒤 EEDR 에 값이 들어옴 */
    return EEDR;
}

static void ee_write_byte(uint16_t addr, uint8_t data)
{
    while (EECR & (1 << EEPE)) { }
    EEAR = addr;
    EEDR = data;

    uint8_t sreg = SREG;                  /* 현재 인터럽트 허용 상태 저장 */
    cli();
    EECR = (1 << EEMPE);                  /* EEPM1:0 = 00 (지우기+쓰기), EEMPE = 1, EEPE = 0 */
    EECR |= (1 << EEPE);                  /* 4 클럭 안에 EEPE = 1 -> 쓰기 시작 (sbi 1명령) */
    SREG = sreg;                          /* 원래 상태로 복원 */
}

/* 값이 다를 때만 쓰기: 수명을 아끼고 시간도 절약 */
static void ee_update_byte(uint16_t addr, uint8_t data)
{
    if (ee_read_byte(addr) != data) ee_write_byte(addr, data);
}

static uint16_t ee_read_u16(uint16_t addr)
{
    return (uint16_t)(ee_read_byte(addr) | ((uint16_t)ee_read_byte(addr + 1) << 8));
}

static void ee_update_u16(uint16_t addr, uint16_t v)
{
    ee_update_byte(addr, (uint8_t)(v & 0xFF));
    ee_update_byte(addr + 1, (uint8_t)(v >> 8));
}

/* 메모리 덤프: 앞 16바이트 */
static void ee_dump16(void)
{
    printf_P(PSTR("EEPROM[0..15]:"));
    for (uint8_t i = 0; i < 16; i++) printf_P(PSTR(" %02X"), ee_read_byte(i));
    printf_P(PSTR("\n"));
}

int main(void)
{
    DDRB  |= (1 << PB2);
    DDRD  &= (uint8_t)~(1 << PD3);
    PORTD |= (1 << PD3);
    uart_init();
    stdout = &uart_out;
    _delay_ms(5);

    printf_P(PSTR("\nlab12 EEPROM boot counter\n"));
    ee_dump16();

    uint16_t boots;
    uint8_t clear_req = (PIND & (1 << PD3)) == 0;

    if (ee_read_byte(EE_ADDR_MAGIC) != EE_MAGIC || clear_req) {
        printf_P(clear_req ? PSTR("button held: reset counter\n")
                           : PSTR("first boot (magic missing): init\n"));
        boots = 0;
        ee_update_u16(EE_ADDR_BOOTS, 0);
        ee_update_byte(EE_ADDR_MAGIC, EE_MAGIC);   /* 매직은 데이터 다음에 마지막으로 씁니다 */
    } else {
        boots = ee_read_u16(EE_ADDR_BOOTS);
    }

    boots++;
    ee_update_u16(EE_ADDR_BOOTS, boots);
    printf_P(PSTR("boot count = %u\n"), boots);
    ee_dump16();

    /* 부팅 횟수만큼 LED 깜박임 (최대 10회) */
    uint8_t n = (boots > 10) ? 10 : (uint8_t)boots;
    for (;;) {
        for (uint8_t i = 0; i < n; i++) {
            PORTB |= (1 << PB2);
            _delay_ms(150);
            PORTB &= (uint8_t)~(1 << PB2);
            _delay_ms(250);
        }
        _delay_ms(1500);
    }
}
