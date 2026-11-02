# Part 8. 밸런싱로봇 SW 개발 with Microchip Studio

Part 3 에서 익힌 주변장치 코드를 "제품 구조"로 다시 짭니다. 핵심은 계층 분리입니다. 레지스터를 만지는 코드(BSW), 부품을 다루는 코드(드라이버), 로봇의 판단을 담은 코드(ASW)를 나누면, 제어 알고리즘을 PC 에서 시뮬레이션으로 먼저 검증하고 보드에는 검증된 코드만 올릴 수 있습니다.

## 8-01 SW 구조 한눈에 보기

```
part08_fw/
  Makefile               avr-gcc 빌드 (Microchip Studio 를 쓰면 참고용)
  src/
    config.h             모든 튜닝 상수, 장착 방향 부호
    main.c               초기화, 협력형 스케줄러 (5 ms, 100 ms)
    bsw/                 Basic SW: 레지스터 직접 제어
      bsw_tick   Timer1 1 ms 틱
      bsw_gpio   방향핀, LED, 버튼
      bsw_pwm    Timer0 Fast PWM (ENA, ENB)
      bsw_uart   USART0 + 송수신 링버퍼 인터럽트
      bsw_i2c    TWI 마스터, 타임아웃
      bsw_adc    ADC 단일 변환
      bsw_eeprom 레지스터 EEPROM, 비차단 쓰기
    drv/                 디바이스 드라이버: 부품 단위
      mpu6050    IMU 초기화, 14 바이트 읽기
      motor      부호 있는 PWM -> 방향핀 + 듀티, 데드존 보상
      battery    배터리 전압 mV, 이동평균
    asw/                 Application SW: 하드웨어 독립 (PC 에서 테스트)
      kalman     2상태 칼만 필터
      pid        PID, 적분 포화 방지
      balance    상태 머신 + 제어 법칙
      protocol   프레임 조립, 파싱, 생성, 체크섬
      cmd        명령 해석
      params     EEPROM 저장 형식, CRC8
  test/                  PC 단위 테스트 + 도립진자 시뮬레이터
```

의존 방향은 항상 아래로만 흐릅니다. ASW 는 `avr/io.h` 를 한 번도 include 하지 않습니다. 이것이 PC 에서 같은 소스를 그대로 컴파일할 수 있는 이유입니다.

| 계층 | 아는 것 | 모르는 것 | 테스트 방법 |
|---|---|---|---|
| ASW | 각도, 각속도, PWM 크기, 메시지 | 레지스터, 핀 번호 | PC 단위 테스트, 시뮬레이터 |
| DRV | 부품의 레지스터 맵, 변환식 | 제어 알고리즘 | 보드 위 단독 확인 (Part 9 TC-01~05) |
| BSW | MCU 레지스터 | 어떤 부품이 붙었는지 | Part 3 실습과 동일 |

## 8-02 Microchip Studio 프로젝트 만들기

1. File > New > Project > **GCC C Executable Project**, 이름 `balancebot`, Device 는 **ATmega328P**.
2. 자동 생성된 `main.c` 를 삭제합니다.
3. Solution Explorer 에서 프로젝트 우클릭 > Add > New Folder 로 `bsw`, `drv`, `asw` 를 만들고, 각 폴더에 우클릭 > Add > Existing Item 으로 이 저장소 `part08_fw/src/` 의 같은 이름 폴더 파일을 모두 추가합니다. `config.h`, `main.c` 는 프로젝트 최상위에 추가합니다.
4. 프로젝트 Properties > Toolchain > AVR/GNU C Compiler
   - Symbols: `F_CPU=16000000UL` 추가
   - Directories: `../bsw`, `../drv`, `../asw`, `..` 추가 (프로젝트 폴더 기준 상대경로)
   - Optimization: `-Os`
   - Miscellaneous: `-std=gnu99`
5. AVR/GNU Linker > Libraries 에 `libm` 추가 (`atan2f` 사용).
6. Build > Build Solution. Output 창에 Program 약 11.5 KB (35 %), Data 약 0.55 KB (27 %) 가 나오면 정상입니다.
7. Tools > Device Programming > Tool **AVRISP mkII**, Device **ATmega328P**, Interface **ISP** > Apply > Read (서명 0x1E950F) > Memories 탭에서 `Debug/balancebot.hex` 를 Program.

Makefile 로 빌드하려면 (리눅스, macOS, WSL):

```
cd part08_fw
make          # build/balancebot.hex
make test     # PC 단위 테스트 + 시뮬레이션
make flash    # avrdude + AVRISP mkII
```

이 저장소에서 실제 빌드한 결과입니다.

```
AVR Memory Usage
Device: atmega328p
Program:   11534 bytes (35.2% Full)
Data:        547 bytes (26.7% Full)
```

## 8-03 설정 파일: config.h

튜닝 상수와 조립 방향 부호를 한 파일에 모읍니다. 모터가 반대로 돌거나 각도 부호가 반대일 때 코드를 고치는 대신 이 파일의 매크로만 바꿉니다.

<!-- include: part08_fw/src/config.h -->

## 8-04 BSW: 시스템 틱 (Timer1)

Part 3 의 lab05 와 같습니다. 32비트 변수를 8비트 CPU 가 네 번에 나눠 읽는 동안 인터럽트가 값을 바꾸면 엉뚱한 값이 나오므로 `ATOMIC_BLOCK` 으로 보호합니다.

<!-- include: part08_fw/src/bsw/bsw_tick.c -->

## 8-05 BSW: GPIO 와 PWM

PWM 에서 놓치기 쉬운 점: Fast PWM 은 OCR 이 0 이어도 매 주기 한 클럭 폭의 펄스가 나옵니다. 그래서 듀티 0 이면 비교출력을 끊고 핀을 LOW 로 둡니다.

<!-- include: part08_fw/src/bsw/bsw_gpio.c -->

<!-- include: part08_fw/src/bsw/bsw_pwm.c -->

## 8-06 BSW: UART 링버퍼

수신은 RX 완료 인터럽트가 버퍼에 쌓고, 송신은 버퍼에 넣은 뒤 UDRE 인터럽트가 내보냅니다. 9600 bps 에서 30 바이트 텔레메트리는 약 31 ms 걸리는데, 폴링 송신이라면 그동안 제어 루프가 6번 멈춥니다. 인터럽트 송신은 이 문제를 없앱니다.

<!-- include: part08_fw/src/bsw/bsw_uart.c -->

## 8-07 BSW: I2C (TWI) 와 타임아웃

센서 선이 흔들려 빠지면 TWINT 를 기다리는 루프가 영원히 끝나지 않을 수 있습니다. 모든 대기에 횟수 제한을 두고, 실패하면 STOP 을 보내고 TWI 를 껐다 켭니다.

<!-- include: part08_fw/src/bsw/bsw_i2c.c -->

