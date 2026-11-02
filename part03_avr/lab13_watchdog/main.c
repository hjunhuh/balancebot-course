/*
 * lab13_watchdog: 워치독 타이머 설정과 리셋 원인(MCUSR) 출력
 *
 * 배선: USB 시리얼(9600 bps), 상태 LED D10 (PB2), 버튼 D3 (PD3, GND 로, 내부 풀업)
 *   정상: 루프가 100 ms 마다 wdt_reset() 으로 워치독을 "먹여" 줍니다.
 *   버튼을 누르고 있으면 무한 루프에 빠진 상황을 흉내 내어 먹이 주기를 멈춥니다.
 *   약 1초 뒤 워치독 리셋이 일어나고, 재부팅 후 "WDRF (watchdog)" 가 출력됩니다.
 *
 * MCUSR (MCU Status Register) 리셋 원인 비트:
 *   bit 3 WDRF  워치독 리셋
 *   bit 2 BORF  브라운아웃 리셋 (전압 강하, Uno 퓨즈 Ext 0xFD = BOD 2.7 V)
 *   bit 1 EXTRF 외부 리셋 (RESET 핀, Uno 의 RESET 버튼, ISP 기록 후 리셋)
 *   bit 0 PORF  전원 켜짐 리셋
 *   비트는 0 을 써야 지워지며, 지우지 않으면 다음 리셋 때도 남아 있습니다.
 *
 * 중요: 워치독 리셋 뒤에는 WDRF 가 1 인 동안 WDE 가 강제로 1 이고, 타임아웃이 가장 짧은 16 ms 로
 *   돌아가 있습니다. main() 에 도달하기 전(전역 변수 초기화 중)에 또 리셋이 날 수 있으므로
 *   .init3 섹션에서 MCUSR 을 저장하고 워치독을 바로 끕니다.
 *
 * WDTCSR 변경 절차 (시간 제한 순서): WDCE 와 WDE 를 동시에 1 로 쓴 뒤 4 클럭 안에 새 값을 씁니다.
 *   <avr/wdt.h> 의 wdt_enable(), wdt_disable() 이 이 절차를 인라인 어셈블리로 정확히 수행합니다.
 */
#include <avr/io.h>
#include <avr/wdt.h>
#include <avr/pgmspace.h>
#include <util/delay.h>
#include <stdio.h>

/* .noinit: C 시작 코드가 0 으로 지우지 않는 영역 */
static uint8_t mcusr_saved __attribute__((section(".noinit")));

/* main 이전, 스택 설정 직후에 실행됩니다 (naked: 프롤로그/에필로그 없음, 호출하지 않음) */
void get_mcusr(void) __attribute__((naked, used, section(".init3")));
void get_mcusr(void)
{
    mcusr_saved = MCUSR;
    MCUSR = 0;
    wdt_disable();
}

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

static void print_reset_cause(uint8_t r)
{
    printf_P(PSTR("MCUSR=0x%02X :"), r);
    if (r & (1 << WDRF))  printf_P(PSTR(" WDRF(watchdog)"));
    if (r & (1 << BORF))  printf_P(PSTR(" BORF(brown-out)"));
    if (r & (1 << EXTRF)) printf_P(PSTR(" EXTRF(reset pin)"));
    if (r & (1 << PORF))  printf_P(PSTR(" PORF(power-on)"));
    if (r == 0)           printf_P(PSTR(" none (jump to 0 or bootloader cleared it)"));
    printf_P(PSTR("\n"));
}

int main(void)
{
    DDRB  |= (1 << PB2);
    DDRD  &= (uint8_t)~(1 << PD3);
    PORTD |= (1 << PD3);
    uart_init();
    stdout = &uart_out;

    printf_P(PSTR("\nlab13 watchdog\n"));
    print_reset_cause(mcusr_saved);

    /* 워치독 리셋으로 깨어났으면 LED 를 3초 켜서 눈으로도 알 수 있게 합니다 */
    if (mcusr_saved & (1 << WDRF)) {
        PORTB |= (1 << PB2);
        _delay_ms(3000);
        PORTB &= (uint8_t)~(1 << PB2);
    }

    /* 타임아웃 1초 (WDP2:0 = 110, 128 kHz 발진기 128K 사이클), 시스템 리셋 모드 */
    wdt_enable(WDTO_1S);
    printf_P(PSTR("WDT enabled (1 s). hold button on D3 to hang\n"));

    uint16_t loops = 0;
    for (;;) {
        if ((PIND & (1 << PD3)) == 0) {
            printf_P(PSTR("hang! no wdt_reset()...\n"));
            for (;;) {                 /* 먹이를 안 주는 무한 루프 = 소프트웨어 멈춤 상황 */
                PINB = (1 << PB2);
                _delay_ms(50);
            }
        }

        wdt_reset();                   /* WDR 명령: 워치독 카운터를 0 으로 */
        PINB = (1 << PB2);
        if ((++loops % 10) == 0) printf_P(PSTR("alive %u\n"), loops);
        _delay_ms(100);
    }
}
