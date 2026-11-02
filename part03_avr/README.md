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

## 3-01 개발환경

### Microchip Studio 7 설치

1. Microchip 홈페이지에서 Microchip Studio for AVR and SAM Devices 7.0 (Windows 전용) 설치 파일을 받습니다.
2. 설치 중 아키텍처 선택에서 **AVR** 을 체크합니다. AVR 8-bit GCC 툴체인, avr-libc, AVRISP mkII 드라이버가 함께 설치됩니다.
3. 설치 후 AVRISP mkII 를 USB 에 꽂고 장치 관리자에서 드라이버가 잡혔는지 확인합니다. (Arduino IDE 용으로 libusb 드라이버를 따로 설치했다면 Microchip Studio 가 장치를 못 찾을 수 있습니다. 이때는 장치 관리자에서 드라이버를 Microchip(Jungo 또는 WinUSB) 쪽으로 되돌립니다.)

### 새 프로젝트 만들기

1. File > New > Project 에서 **GCC C Executable Project** 를 고르고 이름(예: `lab01_blink`)을 정합니다.
2. Device Selection 창에서 `ATmega328P` 를 검색해 선택합니다.
3. 생성된 `main.c` 내용을 지우고 실습 코드(`part03_avr/lab01_blink/main.c`)를 붙여 넣습니다.

### F_CPU 심볼 추가

`<util/delay.h>` 의 `_delay_ms()` 는 `F_CPU` 값으로 반복 횟수를 계산합니다. 정의하지 않으면 경고와 함께 1 MHz 로 가정하여 지연이 16배 짧아집니다.

1. Project > Properties (Alt+F7) 를 엽니다.
2. 위쪽 Configuration 을 **All Configurations** 로 바꿉니다.
3. **Toolchain > AVR/GNU C Compiler > Symbols** 에서 Defined symbols (-D) 옆의 추가 버튼을 누르고 `F_CPU=16000000UL` 을 입력합니다.
4. 같은 Toolchain 의 **Optimization** 에서 Optimization Level 이 `-Os` (Optimize for size) 인지 확인합니다. `_delay_ms()` 는 최적화가 꺼져 있으면 정확하지 않고 코드가 커집니다.

### 빌드

Build > Build Solution (F7) 을 누르고 Output 창에서 `Build succeeded` 와 경고 개수를 확인합니다. 결과물은 `프로젝트폴더\Debug\프로젝트이름.hex` (Release 구성이면 `Release` 폴더)에 생깁니다. 경고도 오류처럼 다룹니다. 이 파트의 모든 실습은 `-Wall -Wextra` 에서 경고 0개로 빌드됩니다.

### AVRISP mkII 연결

AVRISP mkII 는 6핀 ISP 케이블로 MCU 의 SPI 핀(MOSI, MISO, SCK)과 RESET 을 이용해 Flash 를 기록합니다. 표준 6핀 ISP 핀 배치는 다음과 같습니다.

```
   ISP 헤더 (위에서 본 모습)          Uno 쪽 연결
   MISO  1 o o 2  VCC                 1 MISO  = D12 (PB4)
   SCK   3 o o 4  MOSI                3 SCK   = D13 (PB5)
   RESET 5 o o 6  GND                 4 MOSI  = D11 (PB3)
                                      5 RESET = RESET
```

- Uno R3 에는 ISP 헤더가 두 개 있습니다. **USB 커넥터 옆의 ICSP1 은 USB 변환 칩(ATmega16U2)용**이고, 보드 반대쪽 끝(ATmega328P 칩 쪽 가장자리)의 **ICSP 가 ATmega328P 용**입니다. 반드시 ATmega328P 쪽 헤더에 꽂습니다.
- 1번 핀(MISO)은 실크 인쇄의 작은 점이나 `1` 표시로 알 수 있습니다. AVRISP mkII 케이블 커넥터의 빨간 줄(또는 삼각형 표시)이 1번 핀입니다.
- 표시가 잘 안 보이면 멀티미터 통전 모드로 확인합니다: 1번 핀은 D12 와, 3번 핀은 D13 과, 6번 핀은 GND 와 연결되어 있어야 합니다.
- AVRISP mkII 는 타깃에 전원을 공급하지 않습니다. Uno 에 USB 케이블(또는 외부 전원)을 꽂아 두어야 합니다. mkII 의 상태 LED 가 초록이면 타깃 전원이 감지된 것이고, 빨강이면 전원이나 커넥터 방향을 확인합니다.
- D11, D12, D13 에 다른 회로가 무겁게 연결되어 있으면 기록이 실패할 수 있습니다. 그래서 핀맵에서 이 세 핀은 비워 두었습니다.

### Device Programming 으로 기록

1. **Tools > Device Programming** (Ctrl+Shift+P) 를 엽니다.
2. **Tool**: AVRISP mkII, **Device**: ATmega328P, **Interface**: ISP 를 고르고 **Apply** 를 누릅니다.
3. **Interface settings** 에서 **ISP Clock** 을 정합니다. ISP 클럭은 F_CPU/4 이하로 둡니다 (16 MHz / 4 = 4 MHz). 데이터시트의 Serial Programming 절은 12 MHz 이상에서 SCK High/Low 폭을 각각 3 클럭 이상으로 요구하므로 실제 상한은 16 MHz / 6 = 약 2.67 MHz 입니다. 처음에는 125 kHz 정도로 안전하게 시작하고, 잘 되면 1 MHz 정도까지 올려도 됩니다. **Set** 을 눌러 적용합니다.
4. **Device signature** 옆 **Read** 를 누릅니다. `0x1E950F` 가 나오면 ATmega328P 와 정상 통신하는 것입니다. **Target Voltage** 의 Read 로 약 5.0 V 도 확인합니다.
5. **Memories** 탭의 Flash 칸에서 `...` 으로 `.hex` 파일을 고르고, **Erase device before programming** 과 **Verify Flash after programming** 을 체크한 뒤 **Program** 을 누릅니다. 아래 로그에 `Verifying Flash...OK` 가 나오면 끝입니다.
6. 기록이 끝나면 MCU 가 리셋되어 새 프로그램이 바로 실행됩니다.

**퓨즈는 건드리지 않습니다.** Fuses 탭은 읽기만 하고 Program 을 누르지 않습니다. Uno 출고값은 Low 0xFF, High 0xDE, Extended 0xFD 입니다 (도구에 따라 Extended 가 미사용 비트를 빼고 0x05 로 보일 수 있습니다). 퓨즈를 잘못 쓰면 클럭 소스가 바뀌거나 ISP 가 꺼져(SPIEN, RSTDISBL) 일반 ISP 로는 복구할 수 없게 됩니다.

### 부트로더가 지워진다는 점과 복구 방법

ISP 기록은 먼저 칩 전체를 지우므로(Chip Erase) Uno 의 Optiboot 부트로더와 EEPROM 내용(High 퓨즈 0xDE 는 EESAVE 가 꺼져 있음)이 함께 지워집니다.

- 부트로더가 없어도 우리 프로그램은 정상 실행됩니다. High 퓨즈 0xDE 의 BOOTRST 때문에 리셋 직후 부트 섹션(워드 주소 0x3F00)부터 실행하지만, 지워진 Flash(0xFFFF)는 아무 일도 하지 않는 명령처럼 실행되다가 Flash 끝에서 0번지로 돌아와 우리 프로그램이 시작되기 때문입니다.
- USB 케이블은 계속 꽂아 둡니다. 16U2 의 USB 시리얼은 그대로 동작하므로 UART 실습의 출력은 PC 시리얼 모니터에서 볼 수 있습니다.
- 다시 Arduino IDE 의 USB 업로드를 쓰려면 부트로더를 복구합니다.
  - 방법 1: Arduino IDE 에서 툴 > 보드: Arduino Uno, 툴 > 프로그래머: AVRISP mkII 를 고르고 **툴 > 부트로더 굽기** 를 실행합니다 (Windows 에서는 mkII 드라이버 종류 때문에 실패할 수 있습니다).
  - 방법 2: Microchip Studio Device Programming 의 Memories 탭에서 Arduino 설치 폴더의 `hardware/arduino/avr/bootloaders/optiboot/optiboot_atmega328.hex` 를 기록합니다. 퓨즈를 바꾸지 않았다면 이것만으로 복구됩니다.

### Tinkercad Circuits 로 하드웨어 없이 시뮬레이션

보드가 없을 때는 Tinkercad Circuits(웹, 무료)의 Arduino Uno 시뮬레이터로 GPIO, 타이머, PWM, ADC, UART 실습을 해 볼 수 있습니다.

1. Circuits 에서 새 회로를 만들고 Arduino Uno R3, 저항, LED, 버튼, 가변저항(ADC 입력 대용)을 배치합니다.
2. **코드** 버튼을 누르고 블록을 **텍스트** 로 바꿉니다.
3. Tinkercad 는 Arduino 스케치로 컴파일하므로 `main()` 을 직접 쓰지 않고 `setup()` 에 초기화 코드를, `loop()` 에 `for (;;)` 안의 코드를 넣습니다. `#include <avr/io.h>`, `<util/delay.h>`, `<avr/interrupt.h>` 는 그대로 쓸 수 있고 `F_CPU` 는 이미 16 MHz 로 정의되어 있습니다.

```c
#include <avr/io.h>
#include <util/delay.h>

void setup() {
  DDRB |= (1 << PB2);          // lab01 의 main() 앞부분
}

void loop() {
  PINB = (1 << PB2);           // lab01 의 for (;;) 안
  _delay_ms(500);
}
```

주의할 점:

- Arduino 코어가 이미 일부 인터럽트 벡터를 정의합니다. `TIMER0_OVF_vect` 는 `millis()`, `delay()` 용으로 항상 쓰입니다. `Serial` 을 쓰면 `USART_RX_vect`, `USART_UDRE_vect` 가, `attachInterrupt()` 를 쓰면 `INT0_vect`, `INT1_vect` 가 코어에 정의됩니다. 같은 벡터로 `ISR()` 을 또 만들면 "multiple definition" 링크 오류가 납니다. 예를 들어 lab10 의 `ISR(USART_RX_vect)` 를 쓰는 스케치에서는 `Serial` 을 쓰지 말고 UART 레지스터 코드로만 출력합니다.
- Timer0 설정을 바꾸면(lab06, lab07) Arduino 의 `delay()`, `millis()` 가 틀려집니다. 지연은 `_delay_ms()` 를 씁니다.
- 코어의 `init()` 이 `setup()` 전에 Timer1, Timer2 를 PWM 모드로, ADC 를 켠 상태로 만들어 둡니다. 초기화 코드에서 `|=` 대신 `=` 로 레지스터 전체를 써야 우리 설정이 확실하게 들어갑니다.
- Tinkercad 에는 L298N, MPU6050, HC-05 부품이 없습니다. 모터는 L293D 와 DC 모터로 대신하고, lab11, lab14 는 실제 보드에서 확인합니다. EEPROM, 워치독은 시뮬레이터 지원이 완전하지 않을 수 있으므로 실제 보드에서 최종 확인합니다.

---

## 3-02 ChatGPT 활용법

ChatGPT 는 레지스터 코드의 뼈대를 빠르게 만들어 주지만, AVR 계열 칩이 많아서 **다른 칩의 레지스터 이름**을 섞거나 **분주 계산을 틀리는** 일이 자주 있습니다. 그래서 "생성은 AI, 검증은 데이터시트" 원칙으로 씁니다.

### 프롬프트 템플릿

```
역할: 너는 AVR 펌웨어 엔지니어다.
대상: ATmega328P, F_CPU = 16 MHz (외부 크리스탈), avr-gcc + avr-libc, Arduino 라이브러리 사용 금지.
목표: <무엇을 할지 한 문장>
핀: <Uno 핀과 AVR 포트, 예: D10 = PB2>
설정값: <모드, 분주, 주기, 보율 등 이미 정한 값>
요구사항:
 1. 레지스터 이름과 비트 이름은 avr/io.h 에 정의된 ATmega328P 이름만 사용 (예: TIMSK1, UCSR0B).
 2. 레지스터마다 어떤 비트를 왜 켰는지 주석으로 설명.
 3. 계산식(분주, TOP, UBRR 등)을 숫자 대입 과정까지 주석으로 보여 줄 것.
 4. ISR 과 공유하는 변수는 volatile, 16비트 이상 공유 변수는 ATOMIC_BLOCK 으로 읽기.
 5. 블로킹 대기에는 타임아웃을 둘 것.
 6. -Wall -Wextra -std=gnu99 에서 경고 없이 빌드될 것.
출력: main.c 하나, 그리고 데이터시트에서 확인해야 할 레지스터 목록.
```

### 데이터시트 대조 검증 체크리스트

| 항목 | 확인 방법 |
|---|---|
| 레지스터 이름 | `TIMSK`, `UCSRC`, `URSEL`, `GICR` 처럼 ATmega8/16/32 의 이름이 섞이지 않았는지 확인합니다. 328P 는 `TIMSK1`, `UCSR0C`, `EIMSK` 처럼 번호가 붙습니다. 빌드 오류가 나면 거의 이 문제입니다. |
| 비트 위치와 소속 레지스터 | 모드 비트가 두 레지스터에 나뉘는 경우를 특히 봅니다. 예: Timer1 의 WGM11:10 은 TCCR1A, WGM13:12 는 TCCR1B 에 있습니다. Register Description 의 비트 표와 한 비트씩 대조합니다. |
| 모드 표 | WGM, COM, CS, ADPS, ISC 값을 데이터시트 표의 행과 대조합니다. "CTC 모드 4, TOP = OCR1A" 처럼 모드 번호와 TOP 까지 확인합니다. |
| 분주와 주기 계산 | 답에 나온 계산식을 직접 다시 계산합니다 (예: 16 MHz / 64 / 250 = 1000 Hz). 0 부터 세는 레지스터는 `- 1` 이 빠지지 않았는지 봅니다. |
| UART U2X | U2X0 = 1 이면 UBRR 식의 16 이 8 로 바뀝니다. U2X 를 켜면서 16 으로 계산한 코드가 흔합니다. |
| 플래그 지우기 방식 | TIFR1, EIFR, TWCR 의 TWINT 등은 **1 을 써야** 지워집니다. `&= ~` 로 지우는 코드는 틀린 것입니다. |
| 시간 제한 순서 | EEPROM(EEMPE 다음 4 클럭 안에 EEPE), 워치독(WDCE 다음 4 클럭 안에 변경)은 순서와 인터럽트 금지를 확인합니다. |
| ISR 이름 | `SIG_OVERFLOW0` 같은 옛 이름이 아니라 `TIMER0_OVF_vect` 같은 `_vect` 이름인지, 벡터가 328P 에 존재하는지 확인합니다. |
| Arduino 혼용 | `digitalWrite`, `delay`, `Serial`, `Wire` 가 섞이지 않았는지 확인합니다. |
| 빌드와 동작 | 마지막으로 반드시 빌드하고, 실제 보드나 시뮬레이터에서 오실로스코프, LED, 시리얼 출력으로 확인합니다. |

### 실제 프롬프트 예 1: Timer1 1 ms 틱

```
ATmega328P, F_CPU 16 MHz, avr-gcc. Arduino 라이브러리 없이 Timer1 을 CTC 모드로 써서
1 ms 마다 비교 일치 A 인터럽트가 걸리게 하고, 그 안에서 volatile uint32_t 카운터를 올리는
millis() 함수를 만들어 줘. 분주는 64 로 하고 OCR1A 계산 과정을 주석으로 보여 줘.
millis() 는 util/atomic.h 의 ATOMIC_BLOCK 으로 읽어. TCCR1A, TCCR1B, TIMSK1 의 각 비트를
데이터시트 표 기준으로 설명해 줘.
```

검증 포인트: `OCR1A = 249` 인지 (250 이면 0.4 % 느려짐), `WGM12` 가 TCCR1B 에 있는지, `TIMSK` 가 아니라 `TIMSK1` 인지, CS12:10 = 011 인지 확인합니다. 결과는 lab05 와 비교합니다.

### 실제 프롬프트 예 2: UART 9600 bps 와 printf

```
ATmega328P, 16 MHz. USART0 을 9600 bps, 8N1, U2X0 = 1 (2배속)로 설정하는 코드를 작성해 줘.
UBRR0 값을 계산식과 함께 구하고, 실제 보율과 오차(%)도 계산해 줘.
printf 가 UART 로 나가도록 FDEV_SETUP_STREAM 으로 FILE 을 만들어 stdout 에 연결해 줘.
fdevopen 은 쓰지 마. '\n' 앞에 '\r' 을 붙여 줘.
```

검증 포인트: UBRR0 = 16000000 / (8 x 9600) - 1 = 207 이고 오차가 0.16 % 인지, UCSR0C 에 `URSEL` 같은 없는 비트가 없는지, `UCSZ01 | UCSZ00` 로 8비트인지 확인합니다. 결과는 lab09 와 비교합니다.

### 실제 프롬프트 예 3: TWI 로 MPU6050 읽기

```
ATmega328P, 16 MHz, TWI 하드웨어(폴링, 인터럽트 사용 안 함)로 400 kHz I2C 마스터 드라이버를
만들어 줘. 함수는 twi_start(addr, rw), twi_write(byte), twi_read(ack, *byte), twi_stop().
TWINT 대기에는 약 1 ms 타임아웃을 넣고, 각 단계마다 util/twi.h 의 상태 코드(TW_START,
TW_MT_SLA_ACK 등)를 확인해 오류 코드를 돌려줘. 이 드라이버로 MPU6050(주소 0x68)의
WHO_AM_I(0x75)를 읽고, 0x3B 부터 14바이트를 반복 START 로 연속 읽어 빅엔디언 int16_t 로
결합해 줘. TWBR 계산식도 보여 줘.
```

검증 포인트: SCL = 16 MHz / (16 + 2 x TWBR x 분주) 에서 TWBR = 12, TWPS = 0 인지, 상태 코드를 `TWSR & 0xF8` 로 비교하는지, 마지막 바이트를 NACK(TWEA = 0)으로 받는지, START 뒤 주소를 보낼 때 TWSTA 를 지우는지 확인합니다. 결과는 lab11 과 비교합니다.

---

## 실습 공통 사항

- 모든 실습은 USB 케이블로 Uno 에 전원을 공급하고 AVRISP mkII 로 기록합니다.
- UART 출력이 있는 실습은 PC 시리얼 모니터(Arduino IDE 시리얼 모니터, PuTTY, Tera Term 등)를 **9600 bps, 8N1** 로 엽니다. 시리얼 모니터를 열면 16U2 가 DTR 로 리셋을 걸어 프로그램이 처음부터 다시 시작합니다.
- 모터 실습(lab07)을 제외하고 L298N 은 빼 둡니다. D6, D8, D9 를 다른 실습에서 LED 용으로 쓰기 때문입니다.

---

## 3-03 lab01 LED 깜박이기 (DDR, PORT, _delay_ms)

### 배선

| Uno 핀 | AVR | 연결 |
|---|---|---|
| D10 | PB2 | 330 옴 -> LED 애노드(+), LED 캐소드(-) -> GND |

### 핵심 레지스터

| 레지스터 | 비트 | 값 | 의미 |
|---|---|---|---|
| DDRB | DDB2 (bit 2) | 1 | PB2 를 출력으로 |
| PORTB | PORTB2 (bit 2) | 1 / 0 | 출력 High (LED 켜짐) / Low (꺼짐) |
| PINB | PINB2 (bit 2) | 1 쓰기 | PORTB2 반전 (토글) |

### 계산식

- LED 전류: I = (5 V - Vf) / R = (5 - 2.0) / 330 = 약 9 mA. 핀당 권장 20 mA, 절대 최대 40 mA 이하입니다.
- `_delay_ms(500)` 은 F_CPU 를 기준으로 계산된 반복문입니다. 16 MHz 에서 1 ms 는 16,000 클럭입니다.

### 코드

<!-- include: part03_avr/lab01_blink/main.c -->

### 동작 확인

LED 가 0.5초 켜짐, 0.5초 꺼짐을 한 번 한 뒤 0.1초 간격으로 한 번 깜박이는 것을 반복합니다. 스마트폰 스톱워치로 10회 주기를 재어 약 12초인지 확인합니다.

### 자주 하는 실수

- DDRB 를 설정하지 않고 PORTB 만 쓰면 출력이 아니라 내부 풀업만 켜져 LED 가 아주 흐리게 켜집니다.
- F_CPU 를 정의하지 않아 깜박임이 16배 빠릅니다 (3-01 의 Symbols 설정).
- LED 방향(긴 다리가 애노드)을 반대로 꽂습니다.
- `PORTB = (1 << PB2);` 처럼 `=` 를 써서 같은 포트의 다른 핀을 모두 0 으로 만듭니다.

### 연습 문제

1. 켜짐 100 ms, 꺼짐 900 ms 의 "하트비트" 패턴으로 바꾸어 보세요.
2. 방법 A 와 방법 B 의 어셈블리를 `avr-objdump -d main.elf` 로 비교하고 각각 몇 개의 명령인지 세어 보세요.
3. 보드의 L LED(D13 = PB5)도 함께, 반대 위상으로 깜박이게 해 보세요.

---

## 3-04 lab02 비트 연산으로 LED 패턴, 상태 LED

### 배선

| Uno 핀 | AVR | 연결 |
|---|---|---|
| D8 | PB0 | 330 옴 + LED0 -> GND (L298N IN2 자리, L298N 은 빼 둠) |
| D9 | PB1 | 330 옴 + LED1 -> GND (L298N IN4 자리) |
| D10 | PB2 | 330 옴 + LED2 (상태 LED) -> GND |
| D13 | PB5 | 보드 L LED (배선 불필요) |

### 핵심 레지스터와 비트 연산

| 연산 | 코드 | 의미 |
|---|---|---|
| 여러 비트 켜기 | `DDRB \|= LED_MASK` | 마스크의 1 인 비트만 1 로 |
| 여러 비트 끄기 | `PORTB &= ~LED_MASK` | 마스크의 1 인 비트만 0 으로 |
| 비트 교체 | `PORTB = (PORTB & ~MASK) \| out` | 마스크 밖은 보존, 안은 새 값 |
| 비트 반전 | `v ^= 0xF` | 아래 4비트 반전 |
| 시프트 | `1 << i` | i 번 비트만 1 인 값 |

| 레지스터 | 비트 | 값 | 의미 |
|---|---|---|---|
| DDRB | 0, 1, 2, 5 | 1 | LED 4개 출력 (LED_MASK = 0x27) |
| PORTB | 0, 1, 2, 5 | 패턴 | LED 켜짐/꺼짐 |

### 계산식

- LED_MASK = (1<<0) | (1<<1) | (1<<2) | (1<<5) = 0x01 + 0x02 + 0x04 + 0x20 = 0x27
- 상태 블링크 한 주기 = n x (150 + 250) ms + 1000 ms. 상태 3 이면 2.2초입니다.

### 코드

<!-- include: part03_avr/lab02_gpio_out/main.c -->

### 동작 확인

달리는 불빛, 왕복, 0~15 이진 카운터, 0101/1010 교대 패턴이 차례로 나오고, 모든 LED 가 꺼진 뒤 상태 LED(D10)가 세 번 깜박입니다. 이진 카운터에서 D13 이 가장 높은 자리(8)인지 확인합니다.

### 자주 하는 실수

- `~` 연산 결과가 int 로 승격되어 `-Wextra` 경고(부호 변환)가 나옵니다. `(uint8_t)~(...)` 로 형변환합니다.
- `_delay_ms(변수)` 처럼 상수가 아닌 값을 넣습니다. 컴파일 시간 상수만 정확하므로 `delay_ms_var()` 처럼 1 ms 반복으로 만듭니다.
- 포트에 연속되지 않은 핀에 패턴을 그대로 쓰면(`PORTB = v`) 엉뚱한 핀이 켜집니다.

### 연습 문제

1. 4비트 값을 그레이 코드(`n ^ (n >> 1)`)로 출력하는 패턴을 추가해 보세요.
2. `leds_write()` 를 if 문 없이 시프트와 마스크만으로 다시 써 보세요.
3. 상태 번호 0~6(SPEC 의 상태 번호)을 차례로 `status_blink()` 로 보여 주세요.

---

## 3-05 lab03 버튼 입력, 내부 풀업, 소프트웨어 디바운스

### 배선

| Uno 핀 | AVR | 연결 |
|---|---|---|
| D3 | PD3 | 푸시버튼 한쪽, 다른 쪽은 GND (외부 저항 없음) |
| D10 | PB2 | 330 옴 + LED -> GND |
| D13 | PB5 | 보드 L LED (원시 버튼 상태) |

### 핵심 레지스터

| 레지스터 | 비트 | 값 | 의미 |
|---|---|---|---|
| DDRD | DDD3 | 0 | PD3 입력 |
| PORTD | PORTD3 | 1 | 입력일 때 내부 풀업 켜기 |
| PIND | PIND3 | 읽기 | 1 = 안 누름, 0 = 누름 (액티브 로우) |
| MCUCR | PUD (bit 4) | 0 | 1 이면 모든 풀업 비활성 (리셋값 0 유지) |

### 계산식

- 내부 풀업은 20~50 k옴 이므로 누를 때 흐르는 전류는 5 V / 20 k옴 = 최대 약 0.25 mA 입니다.
- 디바운스: 1 ms 마다 샘플링하여 다른 값이 20 회 연속(20 ms)이면 상태 변경으로 인정합니다. 보통 버튼의 채터링은 1~10 ms 정도입니다.

### 코드

<!-- include: part03_avr/lab03_gpio_in/main.c -->

### 동작 확인

버튼을 한 번 누를 때마다 D10 LED 가 정확히 한 번 토글됩니다. 버튼을 누르고 있는 동안 D13 이 켜집니다. `USE_DEBOUNCE` 를 0 으로 바꿔 빌드하면 한 번 눌러도 가끔 두세 번 토글되는(채터링) 것을 볼 수 있습니다.

### 자주 하는 실수

- 풀업을 켜지 않아 입력이 떠 있어서(floating) 손만 가까이 대도 값이 바뀝니다.
- PORTD 를 읽어서 버튼 상태를 판단합니다. 입력 핀의 실제 값은 **PIND** 에서 읽습니다.
- 버튼을 5 V 쪽에 연결하고 풀업을 켭니다. 이 회로는 GND 쪽 연결, 액티브 로우입니다.
- 눌림 "상태"에서 토글하여 누르고 있는 동안 계속 토글됩니다. "엣지"(이전과 다를 때)에서만 동작해야 합니다.

### 연습 문제

1. 버튼을 1초 이상 길게 누르면 LED 를 빠르게 깜박이는 "길게 누름" 기능을 추가해 보세요.
2. 디바운스 시간을 5 ms, 50 ms 로 바꿔 보고 반응성 차이를 설명해 보세요.
3. 0.4초 안에 두 번 누르면 "더블 클릭"으로 인식하게 해 보세요.

---

## 3-06 lab04 외부 인터럽트 INT1 (하강 엣지, volatile)

### 배선

| Uno 핀 | AVR | 연결 |
|---|---|---|
| D3 | PD3 / INT1 | 푸시버튼 -> GND (내부 풀업) |
| D10 | PB2 | 상태 LED (누를 때마다 토글) |
| D13 | PB5 | 보드 L LED (메인 루프 하트비트) |

### 핵심 레지스터

| 레지스터 | 비트 | 값 | 의미 |
|---|---|---|---|
| EICRA | ISC11:ISC10 (bit 3:2) | 10 | INT1 하강 엣지에서 요청 |
| EIMSK | INT1 (bit 1) | 1 | INT1 인터럽트 허용 |
| EIFR | INTF1 (bit 1) | 1 쓰기 | 대기 중인 INT1 플래그 지우기 |
| SREG | I (bit 7) | 1 (`sei()`) | 전역 인터럽트 허용 |

ISC1x 값의 의미: 00 Low 레벨, 01 모든 변화, 10 하강 엣지, 11 상승 엣지.

### 계산식

- 인터럽트 응답 시간: 최소 4 클럭(진입) + ISR 프롤로그. 16 MHz 에서 1 us 정도면 ISR 안으로 들어옵니다. 폴링 방식(lab03)은 루프 한 바퀴(1 ms)까지 늦을 수 있습니다.

### 코드

<!-- include: part03_avr/lab04_ext_interrupt/main.c -->

### 동작 확인

메인 루프는 0.5초마다 D13 을 토글하고, 버튼을 누르는 순간 D10 이 토글됩니다. 메인 루프가 `_delay_ms(10)` 으로 바쁜 중에도 버튼 반응이 즉시 일어나는 것을 확인합니다. AVRISP mkII 는 디버거 기능이 없으므로 `g_isr_count` 와 `handled` 는 lab09 의 UART 출력을 붙여 확인합니다 (연습 문제 4).

### 자주 하는 실수

- 공유 변수에 `volatile` 을 빼서 최적화(-Os)된 메인 루프가 플래그를 레지스터에 한 번만 읽고 다시 읽지 않습니다.
- ISR 안에서 `_delay_ms()` 나 긴 출력을 해서 다른 인터럽트(틱, UART 수신)를 놓칩니다.
- `sei()` 를 빼먹습니다.
- 설정 중에 생긴 INTF1 플래그를 지우지 않아 `sei()` 직후 가짜 인터럽트가 한 번 걸립니다.
- INT1 은 PD3 전용입니다. 다른 핀은 핀 변화 인터럽트(PCINT)를 써야 합니다.

### 연습 문제

1. ISC11:10 을 11(상승 엣지)로 바꾸면 LED 가 언제 토글되는지 확인해 보세요.
2. 메인 루프의 `_delay_ms(50)` 디바운스를 없애고, lab05 의 millis() 로 "마지막 인터럽트 후 50 ms 안의 인터럽트는 무시"하는 방식으로 바꿔 보세요.
3. D2(INT0)에 버튼을 하나 더 달아 두 버튼을 구별해 보세요.
4. lab09 의 UART 코드를 붙여 `g_isr_count` 와 `handled` 를 1초마다 출력하고, 채터링 때문에 ISR 이 누름 횟수보다 더 많이 호출되는 것을 확인해 보세요.

---

## 3-07 lab05 Timer1 CTC 1 ms 틱, millis(), ATOMIC_BLOCK

### 배선

| Uno 핀 | AVR | 연결 |
|---|---|---|
| D10 | PB2 | 상태 LED (500 ms 토글) |
| D13 | PB5 | 보드 L LED (125 ms 토글) |

### 핵심 레지스터

| 레지스터 | 비트 | 값 | 의미 |
|---|---|---|---|
| TCCR1A | WGM11:10 (bit 1:0), COM1x | 0x00 | CTC 의 아래 두 비트 00, 출력 핀 사용 안 함 |
| TCCR1B | WGM13:12 (bit 4:3) | 01 | WGM13:10 = 0100, 모드 4 CTC (TOP = OCR1A) |
| TCCR1B | CS12:10 (bit 2:0) | 011 | 분주 64 (TCCR1B = 0x0B) |
| OCR1A | 16비트 | 249 | 비교 값 (TOP) |
| TIMSK1 | OCIE1A (bit 1) | 1 | 비교 일치 A 인터럽트 허용 |
| TIFR1 | OCF1A (bit 1) | 1 쓰기 | 대기 플래그 지우기 |

### 계산식

```
타이머 클럭  = F_CPU / N = 16,000,000 / 64 = 250,000 Hz (4 us 마다 1 증가)
인터럽트 주파수 = F_CPU / (N x (1 + OCR1A))
OCR1A = F_CPU / (N x f) - 1 = 16,000,000 / (64 x 1000) - 1 = 249
```

- OCR1A 를 250 으로 쓰면 251 카운트가 되어 0.4 % 느려집니다 (하루 약 5.8분).
- `uint32_t` ms 카운터는 2^32 ms = 약 49.7일 뒤에 0 으로 돌아옵니다. `(now - t) >= 주기` 형태의 부호 없는 뺄셈은 이때도 올바르게 동작합니다.

### 코드

<!-- include: part03_avr/lab05_timer_tick/main.c -->

### 동작 확인

D10 은 1초 주기(500 ms 토글), D13 은 250 ms 주기로 깜박입니다. 오실로스코프가 있으면 D13 에서 4.000 Hz 를 확인합니다. 메인 루프에 `_delay_ms()` 가 없는데도 두 LED 주기가 서로 간섭하지 않습니다.

### 왜 ATOMIC_BLOCK 이 필요한가

8비트 AVR 은 32비트 `g_ms` 를 1바이트씩 4번에 나눠 읽습니다. 예를 들어 값이 0x000000FF 에서 0x00000100 으로 바뀌는 순간에 하위 바이트(0xFF)를 읽은 직후 인터럽트가 걸리면, 나머지 바이트는 새 값(0x01)을 읽어 0x000001FF 라는 존재하지 않는 값이 됩니다. `ATOMIC_BLOCK(ATOMIC_RESTORESTATE)` 는 블록 동안 인터럽트를 막고, 끝나면 SREG 를 원래대로 돌려놓습니다.

### 자주 하는 실수

- WGM12 를 TCCR1A 에 씁니다 (TCCR1B 의 bit 3 입니다).
- `TIMSK` (다른 AVR 의 이름)를 써서 빌드 오류가 납니다. 328P 는 `TIMSK1` 입니다.
- `g_ms` 를 ATOMIC_BLOCK 없이 읽어 드물게(수십 초에 한 번) 이상한 값이 나옵니다. 재현이 어려워 찾기 힘든 버그입니다.
- `if (millis() == t)` 처럼 같음 비교를 써서 그 1 ms 를 놓치면 영영 실행되지 않습니다.

### 연습 문제

1. 분주 8 로 바꾸면 1 ms 를 위해 OCR1A 가 얼마여야 하는지 계산하고 바꿔 보세요 (답: 1999).
2. 5 ms (200 Hz, Part 8 의 제어 주기)마다 실행되는 코드를 millis() 로 추가해 보세요.
3. 마이크로초 단위 `micros()` 를 TCNT1 을 이용해 만들어 보세요 (lab15 의 `ticks4us()` 참고).

---

## 3-08 lab06 Timer0 Fast PWM 으로 LED 밝기

### 배선

하드웨어 PWM 은 타이머의 출력 비교 핀에서만 나옵니다. Timer0 의 출력 핀은 OC0A = D6 (PD6), OC0B = D5 (PD5) 입니다. 상태 LED 가 있는 D10 (PB2) 은 Timer1 의 OC1B 이므로 Timer0 PWM 이 나오지 않습니다. **그래서 이 실습에서는 D6 에 LED 를 따로 꽂아 실습합니다.**

| Uno 핀 | AVR | 연결 |
|---|---|---|
| D6 | PD6 / OC0A | 330 옴 + LED -> GND (로봇에서는 L298N ENA, 실습 중 L298N 은 빼 둠) |

### 핵심 레지스터

| 레지스터 | 비트 | 값 | 의미 |
|---|---|---|---|
| TCCR0A | WGM01:00 (bit 1:0) | 11 | WGM02:00 = 011, Fast PWM 8비트, TOP = 0xFF |
| TCCR0A | COM0A1:0 (bit 7:6) | 10 | 비반전: BOTTOM 에서 High, OCR0A 일치에서 Low |
| TCCR0B | WGM02 (bit 3) | 0 | (위 모드의 세 번째 비트) |
| TCCR0B | CS02:00 (bit 2:0) | 010 | 분주 8 (TCCR0B = 0x02) |
| OCR0A | 8비트 | 0..255 | 듀티 |
| DDRD | DDD6 | 1 | OC0A 핀을 출력으로 (안 하면 PWM 이 핀에 안 나옴) |

### 계산식

```
PWM 주파수 = F_CPU / (N x 256) = 16,000,000 / (8 x 256) = 7812.5 Hz
듀티(비반전) = (OCR0A + 1) / 256
```

| 분주 N | PWM 주파수 |
|---|---|
| 1 | 62,500 Hz |
| 8 | 7,812.5 Hz (이 과정의 기준값) |
| 64 | 976.6 Hz (Arduino 기본) |
| 256 | 244.1 Hz |
| 1024 | 61.0 Hz (눈에 깜박임이 보임) |

- OCR0A = 0 이어도 1/256 폭의 펄스가 나가므로, 완전히 끄려면 COM0A 비트를 0 으로 해서 PWM 출력을 끊고 PORTD6 = 0 으로 둡니다 (`pwm_a_set(0)`).
- 감마 보정: out = in^2 / 255. in = 128 이면 out = 65 입니다. 사람 눈은 밝기를 로그적으로 느끼므로 제곱 곡선이 더 자연스럽게 보입니다.

### 코드

<!-- include: part03_avr/lab06_pwm_led/main.c -->

### 동작 확인

LED 가 약 2초 동안 서서히 밝아졌다가 어두워지고, 이어서 0, 25, 50, 75, 100 % 밝기로 1초씩 바뀝니다. 오실로스코프가 있으면 D6 에서 7.81 kHz 사각파와 듀티 변화를 확인합니다. 선형 단계(25 % 와 50 %)의 밝기 차이가 생각보다 작게 느껴지는 것을 관찰합니다.

### 자주 하는 실수

- LED 를 D10 에 꽂고 Timer0 PWM 을 기대합니다. Timer0 PWM 은 D5, D6 에서만 나옵니다.
- DDRD6 을 출력으로 설정하지 않습니다.
- WGM02 가 TCCR0B 에 있다는 것을 놓쳐 모드가 틀어집니다.
- COM0A0 까지 켜서(11) 반전 PWM 이 되어 밝기가 거꾸로 됩니다.

### 연습 문제

1. D5 (OC0B) 에 LED 를 하나 더 달고, 두 LED 가 반대로 숨쉬게 해 보세요.
2. Phase Correct PWM (WGM02:00 = 001) 으로 바꾸면 주파수가 얼마가 되는지 계산하고 확인해 보세요 (답: 16 MHz / (8 x 510) = 약 3921.6 Hz).
3. 감마 보정 결과 256개를 `PROGMEM` 표로 만들어 `pgm_read_byte()` 로 읽는 방식으로 바꿔 보세요.

---

## 3-09 lab07 L298N 모터 구동 (PWM + 방향핀, 램프, 데드존)

### 배선

| Uno 핀 | AVR | L298N | 의미 |
|---|---|---|---|
| D6 | PD6 / OC0A | ENA | 왼쪽 속도 (ENA 점퍼 캡 제거) |
| D7 | PD7 | IN1 | 왼쪽 방향 |
| D8 | PB0 | IN2 | 왼쪽 방향 |
| D5 | PD5 / OC0B | ENB | 오른쪽 속도 (ENB 점퍼 캡 제거) |
| D4 | PD4 | IN3 | 오른쪽 방향 |
| D9 | PB1 | IN4 | 오른쪽 방향 |
| GND | GND | GND | 반드시 공통 접지 |
| D3 | PD3 | (버튼) | 누르고 있으면 비상 정지 |
| D10 | PB2 | (LED) | 모터 동작 중 켜짐 |

왼쪽 모터는 OUT1/OUT2, 오른쪽 모터는 OUT3/OUT4 에 연결하고, 모터 전원은 배터리(7.4 V)를 L298N 의 +12V 단자에 연결합니다. **실습 중에는 바퀴를 바닥에서 띄워 둡니다.**

### L298N 진리표 (왼쪽 기준, 오른쪽은 ENB, IN3, IN4)

| ENA | IN1 | IN2 | 동작 |
|---|---|---|---|
| PWM | 1 | 0 | 정방향 |
| PWM | 0 | 1 | 역방향 |
| 1 | 같음 | 같음 | 급제동 (단자 단락) |
| 0 | x | x | 프리런 (자유 회전 정지) |

### 핵심 레지스터

| 레지스터 | 비트 | 값 | 의미 |
|---|---|---|---|
| TCCR0A | COM0A1:0, COM0B1:0 | 10, 10 | 두 채널 비반전 PWM (명령 0 이면 00 으로 끊음) |
| TCCR0A | WGM01:00 | 11 | Fast PWM 8비트 |
| TCCR0B | CS02:00 | 010 | 분주 8, 7812.5 Hz |
| OCR0A / OCR0B | 8비트 | 0..255 | 왼쪽 / 오른쪽 듀티 |
| PORTD | PD7, PD4 | 방향 | IN1, IN3 |
| PORTB | PB0, PB1 | 방향 | IN2, IN4 |

### 계산식

- 데드존 보상: 명령 크기 m(1..255)을 듀티 d 로 바꿀 때 `d = DZ + m x (255 - DZ) / 255`. DZ = 40 이면 m = 1 -> 41, m = 60 -> 91, m = 128 -> 148, m = 255 -> 255 입니다.
- 램프: 10 ms 마다 최대 5 씩 변하므로 0 -> 200 까지 200 / 5 x 10 ms = 0.4초, 정방향 200 -> 역방향 -200 은 0.8초 걸립니다.
- 모터 평균 전압(대략): Vmotor = (Vbat - L298N 강하) x 듀티. L298N 은 2~3 V 정도 떨어뜨리므로 7.4 V 배터리에서 모터에 걸리는 최대 전압은 약 5 V 입니다.

### 데드존이란

듀티를 0 에서 조금씩 올려도 일정 값까지는 모터가 돌지 않습니다. 정지 마찰과 기어 손실을 이기려면 최소 토크가 필요하고, L298N 의 전압 강하도 있기 때문입니다. 밸런싱 로봇에서 이 구간이 있으면 작은 기울기에 대한 PID 출력이 바퀴를 움직이지 못해 로봇이 흔들리다 넘어집니다. 그래서 0 이 아닌 명령은 데드존 위에서 시작하도록 다시 매핑합니다. DZ 값은 바퀴를 띄운 채 OCR 을 조금씩 올려 막 돌기 시작하는 값을 찾아 정합니다 (보통 30~70).

### 코드

<!-- include: part03_avr/lab07_motor_l298n/main.c -->

### 동작 확인

두 바퀴가 부드럽게 가속하여 전진, 정지, 후진, 제자리 회전, 저속 전진을 반복합니다. 저속 단계(명령 60)에서 바퀴가 확실히 도는지 확인하고, `MOTOR_DEADZONE` 을 0 으로 바꿔 빌드하면 같은 명령에서 바퀴가 "윙" 소리만 내고 서는 것을 비교합니다. 버튼을 누르면 즉시 멈춥니다. 정방향 명령에서 바퀴가 반대로 돌면 `MOTOR_L_INVERT` 또는 `MOTOR_R_INVERT` 를 1 로 바꿉니다.

### 자주 하는 실수

- ENA/ENB 점퍼 캡을 빼지 않아 PWM 과 무관하게 항상 최고 속도로 돕니다.
- Uno 와 L298N 의 GND 를 연결하지 않아 방향핀 신호가 전달되지 않습니다.
- 정방향에서 역방향으로 순간 전환하여 큰 전류가 흐르고 전압이 떨어져 Uno 가 리셋됩니다 (램프가 필요한 이유).
- 7.8 kHz 는 사람 귀에 들리는 주파수라 모터에서 "삐" 소리가 납니다. 정상입니다. 이 값은 L298N 의 스위칭 속도와 소음 사이의 절충입니다.
- L298N 의 5V 출력을 Uno 5V 에 연결한 상태에서 USB 도 꽂아 전원끼리 충돌합니다.

### 연습 문제

1. 바퀴를 띄운 상태에서 왼쪽과 오른쪽 모터의 데드존을 각각 측정하고, 모터별로 다른 값을 쓰도록 바꿔 보세요.
2. 정지 명령에서 프리런 대신 급제동(EN=1, IN1=IN2=0)을 쓰도록 바꾸고 정지 거리를 비교해 보세요.
3. `RAMP_STEP` 을 1 과 50 으로 바꿔 보고, 밸런싱 로봇에서 램프를 너무 느리게 하면 어떤 문제가 생길지 생각해 보세요.

---

## 3-10 lab08 ADC 로 배터리 전압 측정

### 배선

| Uno 핀 | AVR | 연결 |
|---|---|---|
| A0 | PC0 / ADC0 | 배터리(+) - 20 k옴 - A0 - 10 k옴 - GND, A0 와 GND 사이 100 nF |
| GND | GND | 배터리(-) 와 공통 |
| D10 | PB2 | 상태 LED (정상: 켜짐, 저전압: 빠른 깜박임) |

배터리 없이 실습할 때는 가변저항(10 k옴)의 가운데 단자를 A0 에, 양 끝을 5 V 와 GND 에 연결합니다. 이때 출력되는 VBAT 값은 A0 전압의 3배입니다.

### 핵심 레지스터

| 레지스터 | 비트 | 값 | 의미 |
|---|---|---|---|
| ADMUX | REFS1:0 (bit 7:6) | 01 | 기준 전압 AVcc (AREF 핀에 콘덴서, Uno 에 이미 있음) |
| ADMUX | ADLAR (bit 5) | 0 | 결과 오른쪽 정렬 (10비트) |
| ADMUX | MUX3:0 (bit 3:0) | 0000 | ADC0 선택 (ADMUX = 0x40) |
| ADCSRA | ADEN (bit 7) | 1 | ADC 켜기 |
| ADCSRA | ADSC (bit 6) | 1 쓰기 | 변환 시작, 끝나면 0 으로 돌아옴 |
| ADCSRA | ADPS2:0 (bit 2:0) | 111 | 분주 128 (ADCSRA = 0x87) |
| DIDR0 | ADC0D (bit 0) | 1 | PC0 디지털 입력 버퍼 끄기 |
| ADCL/ADCH | | 읽기 | 결과, `ADC` 16비트로 읽기 (ADCL 을 먼저) |

### 계산식

```
ADC 클럭   = 16 MHz / 128 = 125 kHz  (권장 50~200 kHz 범위)
변환 시간   = 13 ADC 클럭 = 104 us (첫 변환은 25 클럭 = 200 us)
분압비     = 10k / (20k + 10k) = 1/3
Vbat[mV]  = ADC x (VREF_MV x 3) / 1024 = ADC x 15000 / 1024   (VREF_MV = 5000)
1 LSB     = 15000 / 1024 = 약 14.6 mV (배터리 기준)
```

| 배터리 | A0 전압 | ADC 값 | 환산 결과 |
|---|---|---|---|
| 8.4 V (만충) | 2.800 V | 573 | 8394 mV |
| 7.4 V (공칭) | 2.467 V | 505 | 7397 mV |
| 6.8 V (예시 임계) | 2.267 V | 464 | 6797 mV |

- 정수 연산: 1023 x 15000 = 15,345,000 은 `uint16_t`(최대 65535)를 넘으므로 `uint32_t` 로 곱한 뒤 1024 로 나눕니다(`>> 10`). `+ 512` 는 반올림입니다. 실수(`float`)를 쓰면 코드가 약 1 KB 늘고 느립니다.
- 분압 저항에 흐르는 전류 8.4 V / 30 k옴 = 0.28 mA 로 배터리 소모는 무시할 수준입니다.
- ADC 입력 임피던스: 분압 출력 저항은 20k 와 10k 의 병렬 = 6.7 k옴 으로 데이터시트 권장 10 k옴 이하이고, 100 nF 가 샘플링 순간의 전하를 공급합니다.

### 코드

<!-- include: part03_avr/lab08_adc/main.c -->

### 동작 확인

시리얼 모니터에 `ADC=505 VBAT=7397 mV` 와 같이 약 0.5초마다 출력됩니다. 멀티미터로 배터리 전압을 재어 출력값과 비교하고, 차이가 크면 Uno 5V 핀 전압을 재서 `VREF_MV` 에 넣습니다 (USB 전원은 4.7~5.1 V 로 흔들립니다).

### 자주 하는 실수

- 분압 없이 7.4 V 배터리를 A0 에 바로 연결해 칩을 망가뜨립니다. 핀 전압은 Vcc + 0.5 V 를 넘으면 안 됩니다.
- 배터리 GND 와 Uno GND 를 연결하지 않아 값이 떠다닙니다.
- `ADMUX = ch;` 처럼 통째로 써서 REFS 비트가 0 (외부 AREF) 이 됩니다.
- ADCH 를 먼저 읽어 결과가 다음 변환까지 잠깁니다. `ADC` 매크로를 쓰면 순서가 맞습니다.
- 정수 곱셈이 16비트에서 넘쳐(오버플로) 엉뚱한 값이 나옵니다.

### 연습 문제

1. 1.1 V 내부 기준(REFS1:0 = 11)으로 바꾸면 이 분압으로 측정 가능한 최대 배터리 전압은 얼마인지 계산해 보세요 (답: 3.3 V, 그래서 분압비를 바꿔야 함).
2. AVcc 를 기준으로 내부 1.1 V 밴드갭(MUX = 1110)을 재서 실제 AVcc 를 역산하는 방법을 구현해 보세요.
3. 전압을 2S 리튬이온 잔량(%)으로 바꾸는 간단한 표(8.4 V = 100 %, 6.8 V = 0 %)를 정수 연산으로 만들어 보세요.

---

