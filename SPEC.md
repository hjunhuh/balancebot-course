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

