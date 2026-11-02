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

