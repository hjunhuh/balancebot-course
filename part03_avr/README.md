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

