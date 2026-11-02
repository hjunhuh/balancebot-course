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

