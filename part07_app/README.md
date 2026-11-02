# Part 7. 밸런싱로봇용 App 개발 with Android Studio

이 파트에서는 휴대폰으로 밸런싱 로봇을 조종하고 PID 게인을 튜닝하는 Android 앱을 만듭니다. 앱은 HC-05 블루투스 모듈과 SPP(시리얼 포트 프로파일)로 연결하고, 공통 사양서(SPEC.md) 4장의 블루투스 프로토콜 v1 을 글자 그대로 따릅니다.

이 폴더의 구성은 다음과 같습니다.

| 경로 | 내용 |
|---|---|
| `app/src/main/java/com/example/balancebot/MainActivity.java` | 화면 하나짜리 앱의 Activity |
| `app/src/main/java/com/example/balancebot/BluetoothService.java` | 블루투스 연결, 송신, 수신 스레드 |
| `app/src/main/java/com/example/balancebot/JoystickView.java` | Canvas 로 직접 그리는 조이스틱 |
| `app/src/main/java/com/example/balancebot/Protocol.java` | 프레임 생성, 체크섬, 파싱 (Android 의존성 없음) |
| `app/src/main/res/layout/activity_main.xml` | 화면 레이아웃 |
| `app/src/main/res/values/strings.xml`, `themes.xml` | 문자열, 테마와 스타일 |
| `app/src/main/AndroidManifest.xml` | 권한과 Activity 선언 |
| `build.gradle`, `settings.gradle`, `gradle.properties`, `app/build.gradle`, `gradle/wrapper/gradle-wrapper.properties` | Gradle 설정 (AGP 8.5.2, Gradle 8.7, Groovy DSL) |
| `test/ProtocolTest.java` | PC 에서 javac 로 돌리는 프로토콜 테스트 |

## 7-01 앱이 하는 일과 화면 설계

앱이 하는 일은 네 가지입니다.

1. 페어링된 HC-05 에 연결하고, 끊기면 화면에 알립니다.
2. 조이스틱으로 로봇을 움직입니다. 터치 중에는 50 ms 마다 `$D,x,y` 를 보내고, 손을 떼면 `$D,0,0` 을 보냅니다.
3. PID 게인(Kp, Ki, Kd), 기준 각도 보정(Trim), 속도 보상 게인(Ks)을 읽고, 바꾸고, EEPROM 에 저장합니다.
4. 로봇이 10 Hz 로 보내는 텔레메트리(`#A`)를 받아 각도, 모터 출력, 배터리 전압, 상태를 보여 줍니다.

화면은 위에서 아래로 한 줄씩 쌓는 단순한 구조입니다. 화면이 작은 휴대폰에서도 잘리지 않도록 전체를 ScrollView 로 감쌉니다. 아래 표가 와이어프레임입니다.

| 순서 | 구역 | 위젯 (id) | 동작 |
|---|---|---|---|
| 0 | 제목 | TextView "BalanceBot" | 표시만 |
| 1 | 연결 | TextView `tvConnState` | "연결 안 됨", "연결 중: HC-05", "연결됨: HC-05" |
| 1 | 연결 | Button `btnConnect` [연결], Button `btnDisconnect` [끊기] (가로 반반) | 권한 확인 후 페어링 목록 다이얼로그, 연결 해제 |
| 2 | 모터 | SwitchCompat `switchMotor` "모터 허용" | 켜면 `$M,1`, 끄면 `$M,0` |
| 3 | 텔레메트리 | `tvAngle` 각도, `tvOutput` 출력 (1행) | `#A` 수신 시 갱신 |
| 3 | 텔레메트리 | `tvBattery` 배터리, `tvState` 상태 (2행) | `#A` 수신 시 갱신 |
| 4 | 조이스틱 | JoystickView `joystick` (240 dp 정사각형) | 터치 중 50 ms 마다 `$D` |
| 4 | 조이스틱 | TextView `tvJoyValue` "x 0, y 0" | 현재 조이스틱 값 |
| 5 | PID 튜닝 | 라벨 + EditText `etKp`, `etKi`, `etKd`, `etTrim`, `etKs` (5행) | 소수 둘째 자리까지 입력 |
| 5 | PID 튜닝 | Button `btnSendParams` [보내기], `btnReadParams` [읽기], `btnSaveParams` [저장] (가로 3등분) | `$P`,`$T`,`$V` 순서 전송 / `$G` / `$S` |
| 6 | 수신 확인 | TextView `tvLastRx` 마지막 수신 줄, `tvRxStats` 정상/무시 줄 수 | 디버그용 |

화면 그림으로 보면 다음과 같습니다.

```
+----------------------------------+
| BalanceBot                       |
| 연결                             |
| 연결됨: HC-05                    |
| [   연결   ]  [   끊기   ]       |
| 모터 허용                  (O-)  |
| 텔레메트리                       |
| 각도: -1.25 도   출력: 40        |
| 배터리: 7.60 V   상태: 3 BALANCE |
| 조이스틱                         |
|            .------.              |
|          /    |     \            |
|         |----(O)----|            |
|          \    |     /            |
|            '------'              |
|            x 0, y 0              |
| PID 튜닝                         |
| Kp  [ 25.00            ]         |
| Ki  [ 0.10             ]         |
| Kd  [ 0.80             ]         |
| Trim[ -1.50            ]         |
| Ks  [ 12.00            ]         |
| [보내기]  [읽기]  [저장]         |
| 수신 확인                        |
| #A,-125,40,7600,3*..             |
| 정상 120 줄, 무시 0 줄           |
+----------------------------------+
```

연결 전에는 [끊기], 모터 스위치, PID 버튼 세 개가 비활성화됩니다. 연결되면 앱이 자동으로 `$G` 를 보내 로봇의 현재 파라미터를 받아 입력칸을 채웁니다.

