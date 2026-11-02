# Part 4. 대기업 개발 프로세스 기반 밸런싱로봇 설계

코드를 쓰기 전에 무엇을 만들지, 어떻게 확인할지를 먼저 문서로 정합니다. 양산 개발 조직(특히 자동차 전장)은 V 모델 기반 프로세스를 따르며, 각 단계 산출물이 다음 단계의 입력이 되고 마지막에 테스트로 되돌아와 검증합니다. 이 파트에서는 그 흐름을 개인 프로젝트 크기로 간소화하여 실제 산출물을 만듭니다. 이 산출물은 Part 10 포트폴리오의 뼈대가 됩니다.

## 4-01 밸런싱 로봇의 원리

밸런싱 로봇은 바퀴 위에 세운 **도립진자**입니다. 막대를 손바닥 위에 세울 때처럼, 막대가 기우는 쪽으로 손(바퀴)을 움직여 받쳐야 서 있을 수 있습니다.

```
        ● 무게중심 (질량 m, 축에서 높이 L)
        |
        |  θ (앞으로 기울면 +)
        |
   =====O=====  바퀴축, 바퀴가 가속도 a 로 움직임
```

선형화한 운동 방정식은 다음과 같습니다.

```
θ'' = (g / L) θ - (1 / L) a
```

- 첫 항 `(g / L) θ`: 기울수록 더 빨리 넘어지려는 성질 (불안정)
- 둘째 항 `-(1 / L) a`: 바퀴를 기우는 쪽으로 가속하면 몸체를 다시 세움
- 넘어지는 빠르기의 척도는 `sqrt(g / L)` 입니다. 이 로봇(Part 6 설계, L 약 0.07 m)은 약 12 rad/s, 즉 0.1 초도 안 되어 눈에 띄게 기웁니다. **무게중심이 높을수록(L 이 클수록) 천천히 넘어져 제어가 쉽습니다.** 그래서 이 과정은 가장 무거운 배터리를 상판에 둡니다.

제어기는 매 5 ms 마다 (1) 센서로 θ 와 θ' 를 추정하고 (2) 필요한 바퀴 출력을 계산해 (3) 모터에 줍니다. 이 순서가 그대로 SW 구조가 됩니다.

## 4-02 부품과 역할

| 부품 | 역할 | 핵심 사양 | 선택 이유 |
|---|---|---|---|
| Arduino Uno (ATmega328P) | 제어 연산 | 16 MHz, Flash 32 KB, SRAM 2 KB | 자료가 많고 레지스터 학습에 적합 |
| MPU6050 (GY-521) | 기울기 측정 | 3축 가속도 + 3축 자이로, I2C | 저렴, 내장 저역통과 필터 |
| L298N | 모터 구동 | 2채널 H 브리지, 최대 2 A/ch | 모듈화되어 배선이 쉬움 (대신 전압 강하 약 2 V) |
| 25 mm 기어드 DC 모터 x2 | 구동 | 축 4 mm | 130 mm 바퀴 허브와 맞음 |
| HC-05 | 무선 명령 | 블루투스 클래식 SPP, UART | 안드로이드에서 시리얼처럼 사용 |
| 18650 x4 (2S2P) | 전원 | 7.4 V 공칭, 8.4 V 만충 | 모터 전압 확보, 용량 2배 |
| 쉴드 PCB (Part 5) | 배선 정리 | 커넥터, 분압, 상태 LED | 점퍼선 접촉 불량 제거 |

## 4-03 V 모델

```
 고객 요구사항 ───────────────────────────────── 인수 테스트 (시연)
     시스템 요구사항 ───────────────────── 시스템 통합 테스트
         HW/SW 요구사항, HSI ─────────── HW/SW 통합 테스트
             SW 아키텍처 설계 ───── SW 통합 테스트
                 SW 상세 설계 ── SW 단위 테스트
                         구현 (코딩)
```

왼쪽은 위에서 아래로 구체화, 오른쪽은 아래에서 위로 검증합니다. 같은 높이의 왼쪽 문서가 오른쪽 테스트의 판정 기준입니다. 이 구조를 따르면 "테스트가 무엇을 확인해야 하는지"가 항상 문서로 정해져 있습니다.

## 4-04 개인 프로젝트용 간소화 프로세스

실무 프로세스(예: 자동차 분야의 ASPICE)는 산출물이 수십 종입니다. 이 과정은 아래 8개만 만듭니다.

| 단계 | 산출물 | 이 저장소 위치 |
|---|---|---|
| 요구 분석 | 고객 요구사항, 시스템 요구사항 | 4-05, 4-06 |
| HW/SW 분할 | HSI 설계서 | 4-07, `part04_design/HSI.csv` |
| SW 설계 | SW 아키텍처, 인터페이스 명세 | 4-08, 4-09 |
| HW 설계 | 회로도, PCB | Part 5 |
| 기구 설계 | 3D 모델, 도면 | Part 6 |
| 구현 | 펌웨어, 앱 | Part 7, Part 8 |
| 검증 | 단위 테스트, 통합 테스트 명세와 결과 | Part 8 `test/`, Part 9 |
| 형상 관리 | Git 저장소, 커밋 이력, MR | 4-10 |

## 4-05 고객 요구사항 (CR)

고객 입장에서 쓰고, 구현 방법은 쓰지 않습니다.

| ID | 요구사항 |
|---|---|
| CR-01 | 손을 떼도 로봇이 스스로 서 있어야 한다. |
| CR-02 | 스마트폰으로 전진, 후진, 회전을 조종할 수 있어야 한다. |
| CR-03 | 스마트폰에서 제어 파라미터를 바꾸고, 전원을 껐다 켜도 유지되어야 한다. |
| CR-04 | 넘어지거나 이상이 생기면 바퀴가 멈춰야 한다. |
| CR-05 | 배터리가 부족하면 스스로 보호하고 알려야 한다. |
| CR-06 | 로봇의 상태를 눈으로 알 수 있어야 한다. |

## 4-06 시스템 요구사항 (SYS) 과 SW 요구사항 (SWR)

시스템 요구사항은 **측정 가능한 숫자**로 씁니다. 숫자가 없으면 통과 여부를 판정할 수 없습니다.

| ID | 시스템 요구사항 | 근거 | 검증 |
|---|---|---|---|
| SYS-01 | 평탄한 바닥에서 손을 뗀 뒤 60 초 이상 넘어지지 않는다. | CR-01 | TC-11 |
| SYS-02 | 정지 밸런싱 중 기울기 진폭은 ±2 도 이내이다 (텔레메트리 기준). | CR-01 | TC-11 |
| SYS-03 | 블루투스 조이스틱으로 전진, 후진, 제자리 회전한다. | CR-02 | TC-12 |
| SYS-04 | 조이스틱 명령이 0.5 초 끊기면 제자리 밸런싱으로 돌아간다. | CR-02, CR-04 | TC-09 |
| SYS-05 | Kp, Ki, Kd, Trim, Ks 를 앱에서 바꾸고 저장하면 재부팅 후에도 유지된다. | CR-03 | TC-07 |
| SYS-06 | 기울기가 45 도를 넘으면 10 ms 이내 모터를 정지한다. | CR-04 | TC-08 |
| SYS-07 | 배터리 6.6 V 미만이 2 초 지속되면 모터를 정지하고 앱에 상태를 표시한다. | CR-05 | TC-10 |
| SYS-08 | LED 로 보정, 대기, 균형, 저전압, 센서 오류를 구분한다. | CR-06 | TC-01, TC-13 |
| SYS-09 | IMU 통신이 3 회 연속 실패하면 모터를 정지하고 1 초마다 재연결한다. | CR-04 | TC-13 |

| ID | SW 요구사항 | 근거 | 담당 모듈 | 검증 |
|---|---|---|---|---|
| SWR-01 | 제어 주기는 5 ms (200 Hz) 이다. | SYS-01 | main, bsw_tick | TC-02 |
| SWR-02 | 기울기는 가속도와 자이로를 칼만 필터로 융합해 추정한다. | SYS-01, 02 | kalman, balance | 단위 테스트 |
| SWR-03 | 부팅 후 1 초 정지 상태에서 자이로 바이어스를 보정하며, 움직이면 다시 시작한다. | SYS-01 | balance | TC-01 |
| SWR-04 | 바퀴 출력은 PID 와 속도 보상으로 계산하며 ±255 로 제한한다. | SYS-01, 03 | pid, balance | 단위 테스트, 시뮬레이션 |
| SWR-05 | 블루투스 프로토콜 v1 (SPEC 4장) 을 구현하고 체크섬 오류 프레임은 버린다. | SYS-03, 05 | protocol, cmd | 단위 테스트, TC-06 |
| SWR-06 | 파라미터는 CRC8 로 보호해 EEPROM 에 저장하며, 손상 시 기본값을 쓴다. | SYS-05 | params, bsw_eeprom | 단위 테스트, TC-07 |
| SWR-07 | EEPROM 쓰기는 제어 루프를 막지 않는다. | SYS-01, 05 | bsw_eeprom | TC-07 |
| SWR-08 | 텔레메트리 #A 를 10 Hz 로 송신하며 송신은 제어 루프를 막지 않는다. | SYS-07, 08 | bsw_uart, main | TC-02 |
| SWR-09 | 워치독 250 ms 를 사용한다. | SYS-06 | main | 코드 리뷰 |
| SWR-10 | I2C 의 모든 대기에는 타임아웃이 있다. | SYS-09 | bsw_i2c | TC-13 |

## 4-07 HSI 설계서 (Hardware Software Interface)

HSI 는 HW 팀과 SW 팀이 "이 핀에 무엇이 어떤 전기적 성질로 연결되고, SW 의 어느 모듈이 어떻게 다루는지"를 합의하는 문서입니다. 회로를 그리기 전에 확정하고, 바뀌면 양쪽이 함께 개정합니다.

| 신호 | Uno 핀 | AVR | 방향 | 전기적 특성 | 연결 | SW 모듈 | 설정 |
|---|---|---|---|---|---|---|---|
| BT_RX | D0 | PD0/RXD0 | In | 3.3 V 로직 입력 (5 V MCU 에서 HIGH 로 인식) | HC-05 TXD | bsw_uart | 9600 8N1, RX 인터럽트 |
| BT_TX | D1 | PD1/TXD0 | Out | 5 V 출력, 1k/2k 분압으로 3.3 V | HC-05 RXD | bsw_uart | UDRE 인터럽트 송신 |
| IMU_INT | D2 | PD2/INT0 | In | 예비 | MPU6050 INT | 없음 (v1) | 입력 |
| BTN | D3 | PD3/INT1 | In | 내부 풀업, 눌림 LOW | 택트 스위치 | bsw_gpio | 100 ms 주기 읽기 |
| MR_IN3 | D4 | PD4 | Out | 5 V 로직 | L298N IN3 | bsw_gpio, motor | |
| MR_PWM | D5 | PD5/OC0B | Out | PWM 7.8 kHz | L298N ENB (점퍼 제거) | bsw_pwm, motor | Timer0 Fast PWM |
| ML_PWM | D6 | PD6/OC0A | Out | PWM 7.8 kHz | L298N ENA (점퍼 제거) | bsw_pwm, motor | Timer0 Fast PWM |
| ML_IN1 | D7 | PD7 | Out | 5 V 로직 | L298N IN1 | bsw_gpio, motor | |
| ML_IN2 | D8 | PB0 | Out | 5 V 로직 | L298N IN2 | bsw_gpio, motor | |
| MR_IN4 | D9 | PB1 | Out | 5 V 로직 | L298N IN4 | bsw_gpio, motor | |
| LED_STAT | D10 | PB2 | Out | 330 옴 직렬, 약 9 mA | 상태 LED | bsw_gpio | |
| VBAT_SENSE | A0 | PC0/ADC0 | Analog | 0..2.8 V (배터리 / 3), 100 nF | 20k/10k 분압 | bsw_adc, battery | AVcc 기준 |
| I2C_SDA | A4 | PC4/SDA | I/O | 오픈드레인, 모듈 풀업 | MPU6050 SDA | bsw_i2c | 400 kHz |
| I2C_SCL | A5 | PC5/SCL | Out | 오픈드레인, 모듈 풀업 | MPU6050 SCL | bsw_i2c | 400 kHz |
| VBAT | VIN | | 전원 | 6.4~8.4 V | 배터리 스위치 뒤 | | |
| 5V | 5V | | 전원 | Uno 레귤레이터 출력 | MPU6050, HC-05 VCC | | |

주의 사항 (HSI 에 함께 적어 둡니다)

- L298N 의 ENA, ENB 점퍼를 반드시 빼야 PWM 이 동작합니다 (점퍼가 있으면 항상 최고 속도).
- L298N 의 5V 출력을 Uno 의 5V 핀에 연결하지 않습니다 (USB 전원과 충돌). 모듈의 12V 입력은 배터리, 5V-EN 점퍼는 그대로 둡니다.
- D0, D1 은 USB 시리얼과 공유됩니다. AVRISP mkII 로 기록하므로 문제없지만, USB 시리얼 모니터와 HC-05 를 동시에 쓰지 않습니다.
- D11, D12, D13 은 ISP 기록 핀이라 이 설계에서는 비워 둡니다.

CSV 원본: `part04_design/HSI.csv`

## 4-08 SW 아키텍처 설계

### 계층 구조

```
+-----------------------------------------------------------+
| ASW  balance | kalman | pid | cmd | protocol | params     |  하드웨어 독립
+-----------------------------------------------------------+
| DRV  mpu6050 | motor | battery                            |  부품 의존
+-----------------------------------------------------------+
| BSW  tick | gpio | pwm | uart | i2c | adc | eeprom        |  MCU 의존
+-----------------------------------------------------------+
| HW   ATmega328P                                           |
+-----------------------------------------------------------+
```

규칙

1. 위 계층은 바로 아래 계층만 호출합니다. 아래 계층은 위 계층을 모릅니다.
2. ASW 는 MCU 헤더(`avr/*.h`)를 include 하지 않습니다. 그래서 PC 에서 단위 테스트와 시뮬레이션을 할 수 있습니다.
3. 인터럽트는 BSW 안에만 있습니다. ISR 은 짧게, 데이터만 버퍼에 넣습니다.
4. 튜닝 상수는 `config.h` 한 곳에만 둡니다.

### 동적 구조 (시간 흐름)

| 주기 | 작업 | 실행 위치 |
|---|---|---|
| 1 ms | 틱 증가 | Timer1 ISR |
| 바이트 수신 시 | 링버퍼 저장 | USART RX ISR |
| 송신 버퍼 비었을 때 | 다음 바이트 송신 | USART UDRE ISR |
| 매 루프 | 워치독 리셋, 명령 처리, EEPROM 한 바이트 진행 | main |
| 5 ms | IMU 읽기, balance_step, 모터 출력 | main 스케줄러 |
| 100 ms | 배터리, 버튼, LED, 텔레메트리 | main 스케줄러 |

### 데이터 흐름

```
MPU6050 --I2C--> mpu6050_read --raw--> imu_to_input --(g, deg/s)--> balance_step
                                                              ^             |
HC-05 --UART--> proto_rx_feed --msg--> cmd_handle --joy, params-+             v
                                                                     motor_set --PWM--> L298N
```

## 4-09 SW 인터페이스 명세 (주요 함수)

| 모듈 | 함수 | 입력 | 출력 | 호출 주기 |
|---|---|---|---|---|
| balance | `balance_step(b, in, out)` | ax, az [g], gy [deg/s], vbat [mV], joy, enable | pwm_l, pwm_r (-255..255) | 5 ms |
| kalman | `kalman_update(k, acc_angle, rate, dt)` | [deg], [deg/s], [s] | 각도 [deg] | 5 ms |
| pid | `pid_update(p, e, d, dt)` | 오차 [deg], 각속도 [deg/s] | 출력 (제한됨) | 5 ms |
| protocol | `proto_rx_feed(rx, c, msg)` | 1 바이트 | NONE, MSG, ERR | 바이트마다 |
| cmd | `cmd_handle(ctx, msg, reply, sz)` | 메시지 | 응답 프레임 길이 | 메시지마다 |
| params | `params_deserialize(p, blob)` | 22 바이트 | 성공 여부 | 부팅 시 |
| motor | `motor_set(l, r)` | -255..255 | 방향핀, PWM | 5 ms |
| mpu6050 | `mpu6050_read(raw)` | | 7개 int16, 성공 여부 | 5 ms |

## 4-10 형상 관리: Git, GitLab, Fork

### 도구

- **Git**: 버전 관리 프로그램
- **GitLab** (또는 GitHub): 원격 저장소, 이슈, Merge Request(MR), CI
- **Fork (Git GUI 클라이언트)**: 커밋 그래프를 보면서 스테이징, 커밋, 브랜치, 머지를 클릭으로 하는 프로그램. 명령어가 익숙하지 않을 때 좋습니다.

### 실습 1: 저장소 만들기

```
git init balancebot
cd balancebot
# 이 과정의 폴더 구조를 복사
git add .
git commit -m "chore: 프로젝트 구조와 공통 사양서 추가"
git remote add origin https://gitlab.com/<내아이디>/balancebot.git
git push -u origin main
```

저장소 루트의 `.gitignore` 는 빌드 산출물(Microchip Studio `Debug/`, Android `build/`, KiCad 백업)을 제외합니다.

### 실습 2: 팀 작업 흐름 (Fork 와 Merge Request)

GitLab 의 "Fork" 는 남의 저장소를 내 계정으로 복사하는 기능입니다. 회사에서는 보통 같은 저장소 안에서 브랜치를 쓰고, 오픈소스나 외부 협력사는 Fork 를 씁니다. 둘 다 연습합니다.

1. GitLab 에서 원본 저장소 > Fork > 내 네임스페이스 선택
2. 내 Fork 를 clone, 기능 브랜치 생성: `git switch -c feature/kalman-tuning`
3. 작업, 커밋 (작게, 자주, 하나의 목적만)
4. `git push -u origin feature/kalman-tuning`
5. GitLab 에서 Merge Request 생성: 원본 저장소의 `main` 으로
6. 리뷰어 코멘트 반영 후 추가 커밋, 승인되면 Merge
7. 원본의 새 변경을 내 Fork 로 가져오기: `git remote add upstream <원본 URL>`, `git fetch upstream`, `git merge upstream/main`

### 커밋 메시지 규칙

```
<type>: <무엇을 왜 바꿨는지 한 줄>

본문 (선택): 배경, 측정 결과, 관련 요구사항 ID
```

| type | 쓰임 | 예 |
|---|---|---|
| feat | 기능 추가 | `feat: $V 명령으로 속도 보상 게인 설정 (SWR-05)` |
| fix | 버그 수정 | `fix: PWM 0 일 때 1클럭 펄스가 나가던 문제 수정` |
| refactor | 동작 같고 구조 개선 | `refactor: I2C 대기 루프를 wait_twint 로 통합` |
| test | 테스트 | `test: 칼만 가짜 평형 테스트 추가` |
| docs | 문서 | `docs: HSI 에 L298N 점퍼 주의사항 추가` |
| chore | 빌드, 설정 | `chore: .gitignore 에 KiCad 백업 추가` |

### 브랜치와 태그

- `main`: 항상 빌드되고 테스트가 통과하는 상태
- `feature/*`, `fix/*`: 작업 브랜치, MR 로만 main 에 합침
- 시연 가능한 버전마다 태그: `git tag -a v1.0 -m "첫 밸런싱 성공"`

### MR 체크리스트 (설명란에 붙여 넣기)

```
- [ ] 관련 요구사항 ID:
- [ ] avr-gcc 빌드 경고 0
- [ ] make test 통과
- [ ] 보드 확인한 TC 번호:
- [ ] config.h 기본값 변경 여부와 이유:
```

