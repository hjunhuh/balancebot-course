# Part 5. 밸런싱로봇 HW 개발 with KiCad

점퍼선으로 연결한 로봇은 진동에 선이 빠져 원인 모를 오동작을 일으킵니다. 이 파트에서는 Arduino Uno 위에 꽂는 쉴드 PCB 를 직접 설계하고 발주합니다. 회로도 작성, 부품 선정, 풋프린트, 배치와 배선, DRC, 거버 생성, 발주까지 실무와 같은 순서로 진행합니다.

## 5-01 전자부품 실무 기초

### 저항

- 옴의 법칙 `V = I R`, 전력 `P = I² R`. 일반 1/4 W 저항은 0.25 W 를 넘기지 않습니다.
- **풀업**: 입력 핀을 평소 HIGH 로 붙잡아 둡니다. 버튼을 누르면 GND 로 떨어집니다. ATmega328P 는 내부 풀업(약 20~50 kΩ)이 있어 외부 저항이 필요 없습니다.
- **분압**: `Vout = Vin x R2 / (R1 + R2)`. 이 쉴드는 두 곳에 씁니다.
  - 배터리 감지: 20k / 10k, 8.4 V 를 2.8 V 로 (ADC 입력 5 V 이하)
  - HC-05 RXD: 1k / 2k, 5 V 를 3.33 V 로 (HC-05 입력은 3.3 V 로직)

### 커패시터

- **디커플링 100 nF**: IC 전원 핀 바로 옆에 두어 순간 전류를 공급하고 고주파 잡음을 GND 로 흘립니다. 길게 돌아가면 효과가 없습니다.
- **벌크 470 uF**: 모터가 갑자기 전류를 끌어갈 때 배터리 전압이 순간적으로 떨어지는 것을 메웁니다. 전해 커패시터는 극성이 있습니다 (긴 다리 +).
- 정격 전압은 실제 전압의 1.5~2 배 이상 (8.4 V 회로에 16 V 이상).

### 다이오드

- **역전압 보호**: 배터리를 반대로 꽂아도 회로가 살도록 직렬로 넣습니다. 쇼트키(1N5822)는 순방향 강하가 약 0.4~0.5 V 로 일반 다이오드(약 0.9 V)보다 작아 모터 전압 손실이 적습니다.
- **LED 전류 계산**: `R = (5 V - Vf) / I = (5 - 2.0) / 0.009 ≈ 330 Ω`.

### 트랜지스터, MOSFET, H 브리지

- MCU 핀은 수십 mA 밖에 못 냅니다. 모터 같은 큰 부하는 트랜지스터나 MOSFET 을 스위치로 써서 켭니다.
- **H 브리지**: 스위치 4개를 H 모양으로 배치해 모터 양단 전압 방향을 바꿉니다. L298N 은 이를 두 채널 담은 IC 입니다. `IN1=1, IN2=0` 이면 정방향, 반대면 역방향, 둘 다 0 이면 자유 정지입니다. EN 핀에 PWM 을 주면 평균 전압이 바뀌어 속도가 바뀝니다.
- L298N 은 바이폴라 트랜지스터라 출력에서 약 2 V 가 떨어집니다. 7.4 V 배터리면 모터에는 약 5 V 남짓이 걸립니다.

### 레귤레이터

- Uno 의 VIN 은 내부 5 V 레귤레이터로 들어갑니다. 입력이 약 7 V 이상이어야 5 V 가 안정적으로 나옵니다. 배터리가 6.6 V 아래로 내려가면 저전압 보호(SYS-07)가 모터를 멈추는 이유 중 하나입니다.

## 5-02 KiCad 9 설치와 화면 구성

1. kicad.org 에서 KiCad 9 를 내려받아 설치합니다 (기본 라이브러리 포함 옵션 선택).
2. 프로젝트 관리자에서 File > New Project, 이름 `balancebot_shield`.
3. 주요 창
   - Schematic Editor: 회로도 (심볼 배치, 배선, ERC)
   - PCB Editor: 기판 (풋프린트 배치, 배선, DRC, 3D 보기)
   - Symbol / Footprint Editor: 라이브러리에 없는 부품 직접 만들기
4. 자주 쓰는 단축키 (회로도): A 심볼 추가, W 배선, L 라벨, P 전원 심볼, M 이동, R 회전, E 속성. (PCB): X 배선, V 비아, D 드래그, F 앞뒤 뒤집기.

## 5-03 KiCad 빠르게 익히기: LED 회로

본 설계 전에 가장 작은 회로로 전 과정을 한 번 돌아 봅니다.

1. Schematic Editor 열기, A 키 > `Device:R` 배치, 다시 A > `Device:LED`, A > `Connector:Conn_01x02` (전원 입력).
2. P 키 > `+5V`, `GND` 전원 심볼 배치.
3. W 키로 +5V > R > LED 애노드, LED 캐소드 > GND, 커넥터 1번 > +5V, 2번 > GND 연결.
4. E 키로 R 값을 `330`, LED 값을 `RED` 로 입력.
5. Tools > Annotate Schematic (R1, D1, J1 번호 자동 부여).
6. Inspect > Electrical Rules Checker 실행. 전원 핀이 구동되지 않았다는 경고가 나오면 `PWR_FLAG` 심볼을 +5V 와 GND 에 붙입니다.
7. Tools > Assign Footprints: R 은 `Resistor_THT:R_Axial_DIN0207_L6.3mm_D2.5mm_P10.16mm_Horizontal`, LED 는 `LED_THT:LED_D5.0mm`, 커넥터는 `Connector_PinHeader_2.54mm:PinHeader_1x02_P2.54mm_Vertical`.
8. PCB Editor 열기 > Tools > Update PCB from Schematic (F8). 부품이 가는 선(ratsnest)으로 연결된 채 나타납니다.
9. Edge.Cuts 레이어 선택 > 사각형으로 30 x 20 mm 외곽 그리기.
10. 부품 배치 후 X 키로 배선, 폭 0.5 mm.
11. Inspect > Design Rules Checker 실행, 오류 0 확인.
12. View > 3D Viewer 로 확인.

## 5-04 PCB 설계 기본 원칙

| 항목 | 이 설계 기준 | 이유 |
|---|---|---|
| 층수 | 2층 | 저렴, 손납땜 쉬움 |
| 신호 배선 폭 | 0.3 mm | 일반 로직 신호 |
| 전원 배선 폭 | VBAT 2.0 mm, 5V 0.8 mm, GND 플레인 | 외층 1 oz 동박 기준 2 A 를 온도 상승 약 10 도 이내로 흘리려면 1 mm 이상이 필요하고, 여유를 둠 |
| 클리어런스 | 0.25 mm 이상 | 대부분 제조사 최소값(약 0.13 mm)에 여유 |
| 비아 | 드릴 0.4 mm, 패드 0.8 mm | 일반 공정 |
| GND | 뒷면(B.Cu) 전체를 GND 구리 영역으로 채움 | 귀환 전류 경로 짧게, 잡음 감소 |
| 디커플링 | 커넥터 VCC 핀에서 5 mm 이내 | 효과는 거리에 비례해 떨어짐 |
| 실크 | 핀 이름, 극성(+), 1번 핀 표시, 보드 이름과 버전 | 조립 실수 방지 |
| 고정 | Uno 홀 4개 위치와 일치 | M3 서포트로 고정 |

모터 전류는 L298N 모듈이 배터리에서 직접 받도록 하고, 쉴드는 배터리를 분배만 합니다. 이렇게 하면 쉴드의 굵은 배선 구간이 짧아집니다.

## 5-05 밸런싱로봇 쉴드 회로도

### 블록 구성

![쉴드 블록 다이어그램](part05_hw/img/shield_block.svg)

### 넷 리스트 (회로도에 그대로 옮깁니다)

| 넷 이름 | 연결 |
|---|---|
| VBAT_IN | J5.1 (배터리 +, 외부 스위치 뒤), D1 애노드 |
| VBAT | D1 캐소드, C1 +, C2.1, J6.1 (L298N +12V 로), J1 VIN, R1.1 |
| GND | J5.2, J6.2, C1 -, C2.2, R2.2, C3.2, R4.2, SW1.2, D2 캐소드, J1 GND, J2.3, J3.2, J4.7, C4.2, C5.2 |
| +5V | J1 5V, J2.2 (HC-05 VCC), J3.1 (MPU VCC), C4.1, C5.1 |
| VBAT_SENSE | R1.2, R2.1, C3.1, J1 A0 |
| BT_TX_5V | J1 D1, R3.1 |
| BT_RXD_3V3 | R3.2, R4.1, J2.5 (HC-05 RXD) |
| BT_RX | J1 D0, J2.4 (HC-05 TXD) |
| BTN | J1 D3, SW1.1 |
| LED_A | J1 D10, R5.1 |
| LED_K_R | R5.2, D2 애노드 |
| SCL | J1 A5, J3.3 |
| SDA | J1 A4, J3.4 |
| IMU_INT | J1 D2, J3.8 |
| ENA | J1 D6, J4.1 |
| IN1 | J1 D7, J4.2 |
| IN2 | J1 D8, J4.3 |
| IN3 | J1 D4, J4.4 |
| IN4 | J1 D9, J4.5 |
| ENB | J1 D5, J4.6 |
| 연결 안 함 | J2.1 (HC-05 EN), J2.6 (STATE), J3.5 XDA, J3.6 XCL, J3.7 AD0 (모듈 내부 풀다운으로 주소 0x68) |

### 커넥터 핀 순서

| 커넥터 | 1 | 2 | 3 | 4 | 5 | 6 | 7 | 8 |
|---|---|---|---|---|---|---|---|---|
| J2 HC-05 (1x06 소켓) | EN | VCC | GND | TXD | RXD | STATE | | |
| J3 MPU6050 GY-521 (1x08 소켓) | VCC | GND | SCL | SDA | XDA | XCL | AD0 | INT |
| J4 L298N 제어 (1x07 핀헤더) | ENA | IN1 | IN2 | IN3 | IN4 | ENB | GND | |
| J5 배터리 입력 (2P 단자대) | VBAT_IN | GND | | | | | | |
| J6 L298N 전원 출력 (2P 단자대) | VBAT | GND | | | | | | |

HC-05 와 GY-521 모듈은 제조사마다 핀 순서가 다를 수 있으니, 가지고 있는 모듈의 실크 인쇄를 보고 J2, J3 순서를 맞춥니다.

### KiCad 에서 Uno 쉴드 만들기

- 심볼: `MCU_Module:Arduino_UNO_R3` 을 배치하면 D0~D13, A0~A5, 전원 핀이 모두 있는 심볼이 나옵니다.
- 풋프린트: `Module:Arduino_UNO_R3_WithMountingHoles` 를 지정하면 Uno 헤더 패드와 고정홀 위치가 정확히 들어옵니다. 쉴드는 이 패드에 긴 핀 헤더(스태커블 헤더)를 납땜해 Uno 위에 꽂습니다.
- 보드 외곽은 이 풋프린트의 외곽선(68.6 x 53.3 mm) 위에 Edge.Cuts 로 그립니다.

## 5-06 부품 선정과 풋프린트 (BOM)

손납땜이 쉬운 스루홀(THT) 부품으로 통일했습니다.

| Ref | 값 | 수량 | KiCad 풋프린트 | 비고 |
|---|---|---|---|---|
| J1 | Arduino UNO R3 | 1 | Module:Arduino_UNO_R3_WithMountingHoles | 스태커블 헤더 (6, 8, 8, 10 핀) |
| J2 | 1x06 소켓 | 1 | Connector_PinSocket_2.54mm:PinSocket_1x06_P2.54mm_Vertical | HC-05 |
| J3 | 1x08 소켓 | 1 | Connector_PinSocket_2.54mm:PinSocket_1x08_P2.54mm_Vertical | GY-521 |
| J4 | 1x07 핀헤더 | 1 | Connector_PinHeader_2.54mm:PinHeader_1x07_P2.54mm_Vertical | L298N 제어선 |
| J5, J6 | 2P 단자대 | 2 | TerminalBlock_Phoenix:TerminalBlock_Phoenix_MKDS-1,5-2_1x02_P5.00mm_Horizontal | 5 mm 피치 |
| D1 | 1N5822 | 1 | Diode_THT:D_DO-201AD_P15.24mm_Horizontal | 3 A 쇼트키, 역전압 보호 |
| D2 | LED 3 mm | 1 | LED_THT:LED_D3.0mm | 상태 LED |
| R1 | 20 kΩ | 1 | Resistor_THT:R_Axial_DIN0207_L6.3mm_D2.5mm_P10.16mm_Horizontal | 분압 상단 |
| R2 | 10 kΩ | 1 | 위와 같음 | 분압 하단 |
| R3 | 1 kΩ | 1 | 위와 같음 | HC-05 RXD 분압 |
| R4 | 2 kΩ | 1 | 위와 같음 | HC-05 RXD 분압 |
| R5 | 330 Ω | 1 | 위와 같음 | LED |
| C1 | 470 uF 16 V | 1 | Capacitor_THT:CP_Radial_D8.0mm_P3.50mm | 벌크 |
| C2, C3, C4, C5 | 100 nF | 4 | Capacitor_THT:C_Disc_D5.0mm_W2.5mm_P5.00mm | 디커플링, ADC 필터 |
| SW1 | 택트 스위치 | 1 | Button_Switch_THT:SW_PUSH_6mm | 모터 허용 토글 |

CSV: `part05_hw/BOM.csv`

