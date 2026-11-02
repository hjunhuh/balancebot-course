# 공통 사양서 (모든 파트가 따르는 기준)

이 문서는 과정 전체에서 공유하는 하드웨어 핀맵, 통신 프로토콜, 폴더 규칙, 작성 규칙을 정의합니다. 파트별 코드와 문서는 반드시 이 값을 그대로 사용합니다.

## 1. 대상 하드웨어

| 항목 | 내용 |
|---|---|
| MCU 보드 | Arduino Uno R3 (ATmega328P, 16 MHz 외부 크리스탈) |
| 프로그래머 | AVRISP mkII (Uno의 6핀 ICSP 헤더에 연결) |
| IDE | Microchip Studio 7 (GCC C Executable Project, Device: ATmega328P) |
| 클럭 | F_CPU = 16000000UL |
| 퓨즈 | 변경하지 않음 (Uno 출고값 Low 0xFF, High 0xDE, Ext 0xFD) |
| IMU | MPU6050 모듈 (GY-521), I2C 주소 0x68 |
| 블루투스 | HC-05 (데이터 모드 9600 bps, 8N1) |
| 모터 드라이버 | L298N 듀얼 H브리지 모듈 |
| 모터 | 25 mm 기어드 DC 모터 (축 4 mm) 2개, 130 mm 바퀴 |
| 배터리 | 18650 4개, 2S2P (공칭 7.4 V, 만충 8.4 V) |

주의: AVRISP mkII로 ISP 기록하면 Uno의 부트로더가 지워집니다. 이후 Arduino IDE의 USB 업로드를 다시 쓰려면 Arduino IDE의 "부트로더 굽기"를 실행합니다.

## 2. 핀맵 (HSI 기준값)

| Uno 핀 | AVR 핀 | 방향 | 신호 | 연결 대상 |
|---|---|---|---|---|
| D0 | PD0 / RXD0 | 입력 | BT_RX | HC-05 TXD |
| D1 | PD1 / TXD0 | 출력 | BT_TX | HC-05 RXD (1k/2k 분압, 5V를 3.3V로) |
| D2 | PD2 / INT0 | 입력 | IMU_INT | MPU6050 INT (예비, v1 미사용) |
| D3 | PD3 / INT1 | 입력 | BTN | 푸시버튼, GND로 연결, 내부 풀업 |
| D4 | PD4 | 출력 | MR_IN3 | L298N IN3 (오른쪽 모터 방향) |
| D5 | PD5 / OC0B | 출력 | MR_PWM | L298N ENB (오른쪽 모터 속도) |
| D6 | PD6 / OC0A | 출력 | ML_PWM | L298N ENA (왼쪽 모터 속도) |
| D7 | PD7 | 출력 | ML_IN1 | L298N IN1 (왼쪽 모터 방향) |
| D8 | PB0 | 출력 | ML_IN2 | L298N IN2 (왼쪽 모터 방향) |
| D9 | PB1 | 출력 | MR_IN4 | L298N IN4 (오른쪽 모터 방향) |
| D10 | PB2 | 출력 | LED_STAT | 상태 LED (330 옴 직렬) |
| A0 | PC0 / ADC0 | 아날로그 | VBAT_SENSE | 배터리 분압 (상단 20k, 하단 10k, 100nF) |
| A4 | PC4 / SDA | 양방향 | I2C_SDA | MPU6050 SDA |
| A5 | PC5 / SCL | 출력 | I2C_SCL | MPU6050 SCL |

모터 정의: 왼쪽 모터 = L298N OUT1/OUT2 (ENA, IN1, IN2), 오른쪽 모터 = OUT3/OUT4 (ENB, IN3, IN4).
방향 규칙: IN1=1, IN2=0 이면 왼쪽 정방향, IN3=1, IN4=0 이면 오른쪽 정방향. 실제 배선에 따라 반대로 돌면 펌웨어 설정 매크로(MOTOR_L_INVERT, MOTOR_R_INVERT)로 맞춥니다.

