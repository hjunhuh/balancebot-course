# Part 3. ATmega328P 프로그래밍 기초 (ChatGPT 활용 포함)

이 파트에서는 Arduino 라이브러리(`digitalWrite`, `analogWrite`, `Serial`, `Wire`)를 쓰지 않고, ATmega328P 의 레지스터를 C 로 직접 다룹니다. 여기서 만드는 GPIO, 타이머, PWM, ADC, UART, I2C(TWI), EEPROM, 워치독 코드는 Part 8 밸런싱 로봇 펌웨어에 거의 그대로 들어갑니다.

실습 코드는 `part03_avr/labNN_이름/main.c` 하나씩이며, 각 파일은 단독으로 빌드됩니다. 모든 핀 배치와 설정값은 과정 공통 사양서(SPEC.md)의 핀맵과 주변장치 기준값을 따릅니다.

| 실습 | 주제 | 주요 레지스터 |
|---|---|---|
| lab01_blink | LED 깜박이기 | DDRB, PORTB, PINB |
| lab02_gpio_out | 비트 연산 LED 패턴, 상태 LED | PORTB |
| lab03_gpio_in | 버튼, 내부 풀업, 디바운스 | PIND, PORTD |
| lab04_ext_interrupt | INT1 하강 엣지 인터럽트 | EICRA, EIMSK, EIFR |
| lab05_timer_tick | 1 ms 틱, millis() | TCCR1A/B, OCR1A, TIMSK1 |
| lab06_pwm_led | Fast PWM LED 밝기 | TCCR0A/B, OCR0A |
| lab07_motor_l298n | 모터 2개, 램프, 데드존 | TCCR0A, OCR0A/B, PORTB/D |
| lab08_adc | 배터리 전압 측정 | ADMUX, ADCSRA, DIDR0 |
| lab09_uart_tx | UART 송신, printf | UBRR0, UCSR0A/B/C, UDR0 |
| lab10_uart_rx_ringbuf | 수신 인터럽트, 링버퍼, 명령 파서 | UCSR0B(RXCIE0) |
| lab11_i2c_mpu6050 | TWI 드라이버, MPU6050 | TWBR, TWSR, TWCR, TWDR |
| lab12_eeprom | EEPROM 직접 접근, 부팅 카운터 | EEAR, EEDR, EECR |
| lab13_watchdog | 워치독, 리셋 원인 | WDTCSR, MCUSR |
| lab14_hc05 | HC-05 블루투스 에코 | UART0 |
| lab15_scheduler | 협력형 스케줄러, 실행시간 측정 | Timer1, TCNT1 |

빌드: `part03_avr` 폴더에서 `make` 를 실행하면 모든 실습이 `avr-gcc -mmcu=atmega328p -DF_CPU=16000000UL -Os -Wall -Wextra -std=gnu99` 로 빌드되고, 각 폴더에 `main.hex` 가 생기며 크기가 출력됩니다. Microchip Studio 에서는 3-01 의 방법으로 프로젝트를 만들고 `main.c` 내용을 붙여 넣습니다.

---

## 3-00 MCU와 ATmega328P 개요

### MCU 란

MCU(Microcontroller Unit)는 CPU, 메모리, 입출력 장치(주변장치)를 칩 하나에 넣은 컴퓨터입니다. PC 의 CPU 와 달리 운영체제 없이 전원이 켜지면 Flash 의 0번지부터 우리가 쓴 프로그램을 바로 실행합니다. 프로그램은 "주변장치 레지스터에 값을 쓰고 읽는 것"으로 하드웨어를 움직입니다.

ATmega328P 의 주요 사양은 다음과 같습니다.

| 항목 | 값 |
|---|---|
| 코어 | 8비트 AVR, 대부분의 명령 1 클럭, 16 MHz 에서 최대 약 16 MIPS |
| Flash (프로그램) | 32 KB (16K 워드), 약 10,000 회 기록 수명 |
| SRAM (변수, 스택) | 2 KB (주소 0x0100..0x08FF) |
| EEPROM (비휘발 데이터) | 1 KB, 약 100,000 회 기록 수명 |
| 타이머 | Timer0 (8비트), Timer1 (16비트), Timer2 (8비트, 비동기 가능) |
| PWM 출력 | 6개 (OC0A, OC0B, OC1A, OC1B, OC2A, OC2B) |
| ADC | 10비트, 6채널 (DIP 패키지 기준) |
| 통신 | USART0 1개, SPI 1개, TWI (I2C) 1개 |
| 동작 전압 | 1.8~5.5 V (16 MHz 에서는 약 3.8 V 이상 필요, Uno 는 5 V) |

### 메모리 구조

AVR 은 하버드 구조입니다. 프로그램 메모리(Flash)와 데이터 메모리(SRAM)가 주소 공간과 버스가 따로입니다.

```
프로그램 메모리 (Flash, 16 비트 워드 단위)      데이터 메모리 (바이트 단위)
0x0000  리셋 벡터, 인터럽트 벡터 26개            0x0000..0x001F  범용 레지스터 R0..R31
0x0034  우리 프로그램 (.text)                   0x0020..0x005F  I/O 레지스터 64개 (in/out, sbi/cbi 가능)
  ...                                          0x0060..0x00FF  확장 I/O 레지스터 160개 (lds/sts 로만 접근)
0x3F00  부트 섹션 (Uno 의 Optiboot, 512 B)       0x0100..0x08FF  내부 SRAM 2 KB
0x3FFF                                                          (.data, .bss, 힙은 아래에서 위로, 스택은 위에서 아래로)

EEPROM: 별도 공간 0x000..0x3FF, EEAR/EEDR/EECR 레지스터로만 접근
```

- `const` 배열도 기본적으로 SRAM 에 복사됩니다. 큰 문자열이나 표는 `PROGMEM`, `PSTR()` 로 Flash 에 두고 `pgm_read_byte()`, `printf_P()` 로 읽어 SRAM 을 아낍니다 (lab09 이후 사용).
- `avr-size` 결과의 `text + data` 가 Flash 사용량, `data + bss` 가 정적 SRAM 사용량입니다. 남은 SRAM 은 스택이 씁니다. SRAM 2 KB 는 생각보다 빨리 찹니다.

### 레지스터

주변장치는 데이터 메모리 공간에 놓인 레지스터로 제어합니다. `<avr/io.h>` 가 레지스터 이름(예: `PORTB`)과 비트 번호(예: `PB2` = 2)를 정의하므로 주소를 외울 필요는 없습니다.

```c
DDRB  |=  (1 << PB2);    /* 비트 켜기 (set) */
PORTB &= ~(1 << PB2);    /* 비트 끄기 (clear) */
PORTB ^=  (1 << PB2);    /* 비트 반전 (toggle) */
if (PIND & (1 << PD3))   /* 비트 검사 (test) */
```

레지스터 하나에는 의미가 다른 비트들이 함께 들어 있으므로 `=` 로 통째로 쓸지 `|=`, `&=` 로 일부만 바꿀지 항상 의식해야 합니다. 초기화 함수에서는 의도한 값을 `=` 로 확실하게 쓰고, 동작 중에는 필요한 비트만 바꾸는 것이 기본 원칙입니다.

### 데이터시트 보는 순서

참고 문서는 Microchip 의 "ATmega48A/PA/88A/PA/168A/PA/328/P" 데이터시트(문서 번호 DS40002061)입니다. 판(revision)마다 장 번호가 다르므로 이 강의에서는 장 이름으로 부릅니다. 600쪽이 넘으므로 처음부터 읽지 않고 다음 순서로 필요한 부분만 찾습니다.

1. **Pin Configurations**: 쓰려는 기능이 몇 번 핀에 있는지, 그 핀의 다른 기능은 무엇인지 확인합니다.
2. **해당 주변장치 장의 Overview, 블록 다이어그램**: 신호가 어디서 어디로 가는지 그림으로 이해합니다.
3. **동작 설명 (Modes of Operation 등)**: 모드별 동작과 계산식(주파수, 보율)을 찾습니다.
4. **Register Description**: 레지스터마다 비트 표, 리셋 초깃값, 읽기/쓰기 속성(R, W, R/W)을 확인합니다. 모드 선택 표(예: WGM 표, 분주 표)도 여기 있습니다.
5. **Electrical Characteristics**: 전류, 전압, 타이밍 한계를 확인합니다.
6. **코드 예제 (Assembly Code Example, C Code Example)**: 많은 장에 공식 예제가 있으므로 ChatGPT 답과 대조할 기준으로 씁니다.

### Uno 핀과 AVR 핀 대응표

| Uno 핀 | AVR 포트 | DIP 핀 번호 | 대체 기능 | 이 과정의 용도 (SPEC) |
|---|---|---|---|---|
| D0 | PD0 | 2 | RXD0 | BT_RX (HC-05 TXD) |
| D1 | PD1 | 3 | TXD0 | BT_TX (HC-05 RXD) |
| D2 | PD2 | 4 | INT0 | IMU_INT (예비) |
| D3 | PD3 | 5 | INT1, OC2B | BTN (버튼) |
| D4 | PD4 | 6 | T0, XCK | MR_IN3 |
| D5 | PD5 | 11 | OC0B, T1 | MR_PWM (ENB) |
| D6 | PD6 | 12 | OC0A, AIN0 | ML_PWM (ENA) |
| D7 | PD7 | 13 | AIN1 | ML_IN1 |
| D8 | PB0 | 14 | ICP1, CLKO | ML_IN2 |
| D9 | PB1 | 15 | OC1A | MR_IN4 |
| D10 | PB2 | 16 | OC1B, SS | LED_STAT |
| D11 | PB3 | 17 | OC2A, MOSI | ISP 전용 (비워 둠) |
| D12 | PB4 | 18 | MISO | ISP 전용 (비워 둠) |
| D13 | PB5 | 19 | SCK | 보드 L LED, ISP |
| A0 | PC0 | 23 | ADC0 | VBAT_SENSE |
| A1..A3 | PC1..PC3 | 24..26 | ADC1..ADC3 | 미사용 |
| A4 | PC4 | 27 | ADC4, SDA | I2C_SDA |
| A5 | PC5 | 28 | ADC5, SCL | I2C_SCL |
| RESET | PC6 | 1 | RESET | ISP |
| (크리스탈) | PB6, PB7 | 9, 10 | XTAL1, XTAL2 | 16 MHz 크리스탈 |

---

