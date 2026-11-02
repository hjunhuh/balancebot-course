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

## 8-08 BSW: ADC 와 EEPROM

EEPROM 한 바이트 쓰기는 약 3.3 ms 입니다. 파라미터 22 바이트를 한 번에 쓰면 73 ms 동안 제어가 멈춰 로봇이 넘어집니다. 그래서 메인 루프가 돌 때마다 한 바이트씩 진행하는 비차단 방식으로 만들었습니다.

<!-- include: part08_fw/src/bsw/bsw_adc.c -->

<!-- include: part08_fw/src/bsw/bsw_eeprom.c -->

## 8-09 드라이버: MPU6050

| 레지스터 | 값 | 이유 |
|---|---|---|
| PWR_MGMT_1 (0x6B) | 0x01 | 슬립 해제, 자이로 PLL 클럭 |
| SMPLRT_DIV (0x19) | 4 | 200 Hz, 제어 주기와 맞춤 |
| CONFIG (0x1A) | 3 | 약 44 Hz 저역통과, 모터 진동 억제 |
| GYRO_CONFIG (0x1B) | 0x00 | ±250 deg/s, 131 LSB/(deg/s) |
| ACCEL_CONFIG (0x1C) | 0x00 | ±2 g, 16384 LSB/g |

<!-- include: part08_fw/src/drv/mpu6050.c -->

### 각도와 축의 부호

SPEC 장착 기준은 X 축이 로봇 앞, Z 축이 위, Y 축이 왼쪽입니다. 로봇이 앞으로 θ 만큼 기울면 가속도계는 `ax = -sin θ`, `az = cos θ` (단위 g) 를 읽습니다. 그래서 `θ = atan2(-ax, az)` 이고, 앞으로 숙이는 회전은 Y 축 기준 양의 회전이라 자이로 `gy` 가 + 가 됩니다. 장착 방향이 다르면 `IMU_ANGLE_SIGN` 을 -1 로 바꾸거나 `main.c` 의 `imu_to_input` 에서 축을 바꿉니다.

## 8-10 드라이버: 모터와 데드존

DC 모터는 PWM 이 어느 값 이하이면 정지마찰 때문에 돌지 않습니다. 제어기가 작은 값을 내도 바퀴가 반응하도록 1..255 를 40..255 로 다시 펼칩니다.

<!-- include: part08_fw/src/drv/motor.c -->

<!-- include: part08_fw/src/drv/battery.c -->

## 8-11 칼만 필터

가속도계는 오래 보면 정확하지만 진동에 약하고, 자이로는 순간 변화에 정확하지만 적분하면 바이어스 때문에 흘러갑니다. 칼만 필터는 두 센서를 상황에 맞는 비율로 섞습니다.

| 단계 | 식 | 의미 |
|---|---|---|
| 예측 | angle += dt (gyro - bias) | 자이로로 다음 각도 예상 |
| 예측 | P = A P Aᵀ + Q | 예상이 얼마나 불확실해졌는지 |
| 보정 | K = P Hᵀ / (H P Hᵀ + R) | 가속도계를 얼마나 믿을지 |
| 보정 | angle += K0 (acc_angle - angle), bias += K1 (...) | 오차만큼 당겨 옴 |

<!-- include: part08_fw/src/asw/kalman.c -->

## 8-12 PID 제어

```
u = Kp e + Ki ∫e dt + Kd (dθ/dt),   e = θ - θ_target
```

- Kp: 기울어진 만큼 바퀴를 굴립니다. 클수록 빨리 세우지만 너무 크면 빠르게 떱니다.
- Kd: 넘어지는 속도에 반응하는 감쇠입니다. 오차를 미분하지 않고 자이로 각속도를 바로 씁니다 (목표가 바뀔 때 튀지 않음, 잡음 적음).
- Ki: 오래 남는 기울기를 없앱니다. PWM 이 사실상 바퀴 속도를 정하는 DC 모터에서는 Ki 가 "버티는 힘" 역할을 크게 맡습니다 (8-14 참고).
- 적분 포화 방지: `Ki * integ` 가 출력 한계를 넘지 않게 적분값을 묶습니다.

<!-- include: part08_fw/src/asw/pid.c -->

## 8-13 상태 머신과 제어 법칙

```
INIT -> CALIB -> WAIT_UPRIGHT -> BALANCE <-> FALLEN
                       ^             |
                       +---- IDLE <--+  (모터 허용 끔)
              어느 상태든 -> LOW_BATT (배터리 6.6 V 미만 2 s)
```

- CALIB: 전원을 켜고 1 초 동안 가만히 두면 자이로 바이어스를 평균으로 구합니다. 흔들리면 처음부터 다시 셉니다. LED 가 빠르게 깜빡입니다.
- WAIT_UPRIGHT: 손으로 세워 기준각 ±5 도 안에 0.3 초 머물면 제어를 시작합니다.
- BALANCE: 목표 각도 = 트림 + 조이스틱 전진량 x 3 도 - Ks x (속도 추정 / 255). 속도가 붙으면 목표를 뒤로 눕혀 멈추게 하는 바깥 루프입니다. 엔코더가 없으므로 모터 출력을 저역통과한 값을 속도 대신 씁니다.
- FALLEN: 45 도를 넘으면 모터를 끕니다. 다시 세우면 자동으로 BALANCE 로 돌아갑니다.

<!-- include: part08_fw/src/asw/balance.c -->

## 8-14 시뮬레이터로 먼저 검증하기 (중요)

`test/sim_balance.c` 는 펌웨어의 `balance.c`, `kalman.c`, `pid.c`, `motor.c` 를 그대로 링크하고, 단순화한 도립진자와 DC 모터 모델 위에서 돌립니다. 보드에 올리기 전에 부호 규칙, 상태 전이, 게인의 크기 범위를 확인할 수 있습니다.

```
cd part08_fw/test
make sim_balance
./sim_balance                      # config.h 기본 게인
./sim_balance 20 50 0.5 10         # Kp Ki Kd Ks 직접 지정
SIM_TRACE=1 ./sim_balance          # 50 ms 마다 상태 출력
SIM_IDEAL=1 ./sim_balance          # 센서 오차 없이 제어 법칙만 확인
SIM_L=0.13 SIM_TAU=0.15 ./sim_balance   # 무게중심 높이, 모터 응답 바꿔 보기
```

시뮬레이터의 기본 로봇은 Part 6 기구 설계값을 따릅니다 (바퀴축에서 무게중심까지 약 0.07 m, 모터 응답 0.1 s, 최고 속도 1.2 m/s). 기본 게인 실행 결과입니다.

```
gains: Kp 15.00  Ki 100.00  Kd 0.25  Ks 5.00
  [제자리 10s] OK    max|theta| 0.30 deg, max|x| 0.33 m, 마지막 2s 평균속도 -0.048 m/s
  [충격 60dps] OK    max|theta| 2.08 deg, max|x| 0.39 m, 마지막 2s 평균속도 -0.031 m/s
  [전진 y=60] OK    max|theta| 4.05 deg, max|x| 1.49 m, 마지막 2s 평균속도 +0.561 m/s
  [큰 충격 400dps] t=1.41s 넘어짐 판정 (theta 46.7 deg)
  [큰 충격 400dps] OK   넘어짐을 감지하고 모터 정지
result: PASS
```

기본 게인은 무게중심 높이 3 가지(0.07, 0.10, 0.13 m) x 모터 응답 3 가지 x 최고 속도 3 가지, 모두 27 가지 로봇 조건으로 게인 조합 수백 개를 돌려 가장 많은 조건(27 중 21)을 통과한 값으로 정했습니다. 실패한 조건은 대부분 모터가 아주 빠른 경우(최고 속도 1.5 m/s, 응답 0.06 s)로, 이때는 Kp 를 낮추면 됩니다.

### 시뮬레이터가 찾아낸 설계 결정 두 가지

**첫째, 인터넷에 흔한 칼만 상수는 이 로봇에서 실패합니다.** 많은 예제가 `Q_bias = 0.003`, `R = 0.03` 을 씁니다. 이 값은 가속도계를 강하게 믿습니다. 그런데 로봇이 기운 채로 앞으로 가속하면, 가속도계가 느끼는 합력이 몸체 축과 나란해져 "똑바로 서 있다"고 읽힙니다 (가짜 평형). 필터가 이 값을 믿으면 추정 각도가 실제보다 작아지고, 제어기는 이미 많이 기운 로봇을 덜 기울었다고 판단해 결국 넘어집니다. `sweep_kalman.sh` 로 게인 108 조합 x 무게중심 높이 3 가지 = 324 경우를 돌린 결과입니다.

```
Q_BIAS=0.003 R=0.03 : 통과 2 / 324
Q_BIAS=0.003 R=3.0 : 통과 159 / 324
Q_BIAS=0.0001 R=3.0 : 통과 200 / 324
Q_BIAS=0.00001 R=30.0 : 통과 193 / 324
```

그래서 부팅 때 자이로 바이어스를 먼저 평균으로 빼고, 필터에는 작은 `Q_bias` 와 큰 `R` 을 주어 자이로를 더 믿게 했습니다. 단위 테스트 `test_pid_kalman` 의 "가짜 평형" 항목이 이 성질을 계속 감시합니다.

**둘째, Kp 와 Kd 만으로는 서지 못합니다.** PWM 을 주면 DC 모터는 "힘"보다 "속도"를 정하는 쪽에 가깝습니다. 이때 Kp 는 주로 감쇠 역할을 하고, 넘어지려는 힘을 이기는 복원력은 Ki 와 속도 루프(Ks)에서 나옵니다. `SIM_IDEAL=1` 로 센서 오차를 없애도 Ki = 0, Ks = 0 조합은 통과하지 못합니다. 실제 로봇에서 Kp 만 올리다 "멈추지 않고 한쪽으로 달려가다 넘어지는" 현상이 바로 이것입니다.

시뮬레이터 모델은 단순화되어 있습니다. 실제 로봇의 게인은 Part 9 의 튜닝 절차로 다시 찾지만, 시작점과 부호가 맞는다는 확신을 가지고 시작할 수 있습니다.

<!-- include: part08_fw/test/sim_balance.c -->

## 8-15 블루투스 프로토콜 구현

SPEC 4장의 프레임을 바이트 단위로 조립합니다. 시작 문자를 받으면 언제든 새 프레임으로 다시 동기화하므로 잡음이 끼어도 다음 프레임부터 정상 처리됩니다. `sscanf` 를 쓰지 않아 코드가 작고 동작이 예측 가능합니다.

| 받은 줄 | 결과 |
|---|---|
| `$G*47` | 정상, 파라미터 보고 `#K,...` |
| `$M,1*50` | 정상, 모터 허용, `#O,M` |
| `$M,1*51` | 체크섬 오류 `#E,1` |
| `$M,*61` | 빈 필드, 형식 오류 `#E,2` |
| `$P,-1,0,0*..` | 범위 오류 `#E,3`, 값 유지 |
| `$Z*5A` | 알 수 없는 명령 `#E,4` |

<!-- include: part08_fw/src/asw/protocol.c -->

<!-- include: part08_fw/src/asw/cmd.c -->

## 8-16 EEPROM 파라미터 저장

| 주소 | 크기 | 내용 |
|---|---|---|
| 0 | 2 | magic 0xB0B1 |
| 2 | 1 | version 1 |
| 3 | 4 x 4 | kp, ki, kd, ks (x100) |
| 19 | 2 | trim (x100 도) |
| 21 | 1 | CRC8 (다항식 0x07) |

처음 부팅한 보드의 EEPROM 은 모두 0xFF 라 magic 이 맞지 않습니다. 이때와 CRC 가 틀릴 때, 범위를 벗어날 때는 모두 기본값을 씁니다. 앱에서 `$S` 를 보내면 비차단 쓰기가 끝난 뒤 `#O,S` 와 `#K` 를 보냅니다.

<!-- include: part08_fw/src/asw/params.c -->

## 8-17 main: 협력형 스케줄러

<!-- include: part08_fw/src/main.c -->

### 타이밍 예산 (5 ms 제어 주기 안)

| 작업 | 시간 (대략) |
|---|---|
| I2C 14 바이트 읽기 @400 kHz | 약 0.45 ms |
| atan2f + 칼만 + PID (소프트웨어 부동소수점) | 약 0.5 ms |
| 모터 출력 | 수 us |
| 합계 | 약 1 ms, 여유 80 % |

실제 보드에서 확인하려면 `task_control` 앞뒤로 LED 핀을 HIGH/LOW 로 바꾸고 오실로스코프나 로직 분석기로 펄스 폭을 봅니다.

## 8-18 PC 단위 테스트

```
cd part08_fw && make test
```

실행 결과 (이 저장소에서 실제 실행):

```
  built: $D,-35,100*5E
  ack:   #O,S*30
  #K:    #K,3000,150,90,0,500*5C
[protocol] pass 34, fail 0
[params] pass 6, fail 0
  kalman static: angle 10.414 deg, bias 0.031 deg/s
  kalman ramp: max error 0.540 deg (acc noise +-5 deg)
  kalman fake-equilibrium: truth 5.00, estimate after 1 s 4.841 deg
[pid_kalman] pass 7, fail 0
result: PASS
```

<!-- include: part08_fw/test/test_protocol.c -->

<!-- include: part08_fw/test/test_pid_kalman.c -->

