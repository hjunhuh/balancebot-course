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

## 7-02 Android Studio 설치와 새 프로젝트 만들기

### Android Studio 설치

1. https://developer.android.com/studio 에서 Android Studio 를 내려받아 설치합니다. Windows 는 설치 마법사의 기본값 그대로 진행하면 됩니다.
2. 처음 실행하면 Setup Wizard 가 나옵니다. Standard 를 고르면 Android SDK, SDK Platform-Tools, 에뮬레이터, 최신 Android SDK Platform 이 함께 설치됩니다.
3. 설치 후 Welcome 화면의 More Actions, SDK Manager 에서 SDK Platforms 탭의 Android 14.0 (API 34) 이 설치되어 있는지 확인합니다. 없으면 체크하고 Apply 를 누릅니다.
4. 같은 창의 SDK Tools 탭에서 Android SDK Build-Tools, Android SDK Platform-Tools 가 설치되어 있는지 확인합니다.

블루투스는 에뮬레이터에서 동작하지 않으므로 이 파트의 테스트는 반드시 실제 휴대폰으로 합니다.

### 새 프로젝트 만들기

1. Welcome 화면에서 New Project 를 누릅니다.
2. Phone and Tablet 탭에서 **Empty Views Activity** 를 고릅니다. (Empty Activity 는 Jetpack Compose 템플릿이므로 고르지 않습니다.)
3. 다음 값을 입력합니다.

| 항목 | 값 |
|---|---|
| Name | BalanceBot |
| Package name | com.example.balancebot |
| Save location | 원하는 폴더 |
| Language | Java |
| Minimum SDK | API 23 ("Marshmallow"; Android 6.0) |
| Build configuration language | Groovy DSL (build.gradle) 권장, Kotlin DSL 이어도 아래 방법으로 그대로 됩니다 |

4. Finish 를 누르고 Gradle Sync 가 끝날 때까지 기다립니다 (처음에는 몇 분 걸립니다). 이 상태에서 한 번 Run 해서 빈 화면 앱이 뜨는지 확인해 두면 이후 문제를 나누어 보기 좋습니다.

Name 을 꼭 `BalanceBot` 으로 하세요. 템플릿이 만드는 테마 이름이 `Theme.BalanceBot` 이 되고, 이 폴더의 매니페스트도 같은 이름을 씁니다.

### 이 폴더의 파일로 덮어쓰기

Gradle 파일까지 통째로 덮어쓰면 Android Studio 버전에 따라 버전 카탈로그(libs.versions.toml)나 Kotlin DSL 과 섞여 동기화가 실패하기 쉽습니다. 가장 확실한 방법은 **Android Studio 가 만든 프로젝트와 Gradle 설정은 그대로 두고, 소스와 리소스만 교체**하는 것입니다. Empty Views Activity 템플릿에는 이미 appcompat 과 material 의존성이 들어 있으므로 Gradle 은 건드릴 필요가 없습니다.

Android Studio 왼쪽 위 보기 방식을 Android 에서 **Project** 로 바꾸면 실제 폴더 구조가 보입니다. 아래 표대로 복사합니다 (`<프로젝트>` 는 새로 만든 BalanceBot 폴더).

| 이 폴더의 파일 | 복사할 위치 | 방법 |
|---|---|---|
| `app/src/main/java/com/example/balancebot/MainActivity.java` | `<프로젝트>/app/src/main/java/com/example/balancebot/` | 덮어쓰기 |
| `BluetoothService.java`, `JoystickView.java`, `Protocol.java` | 같은 폴더 | 새로 추가 |
| `app/src/main/res/layout/activity_main.xml` | `<프로젝트>/app/src/main/res/layout/` | 덮어쓰기 |
| `app/src/main/res/values/strings.xml` | `<프로젝트>/app/src/main/res/values/` | 덮어쓰기 |
| `app/src/main/res/values/themes.xml` | `<프로젝트>/app/src/main/res/values/` | 덮어쓰기 |
| `app/src/main/AndroidManifest.xml` | `<프로젝트>/app/src/main/` | 덮어쓰기 |

복사한 다음 아래 정리를 합니다.

1. `<프로젝트>/app/src/main/res/values-night/themes.xml` 을 삭제합니다. 템플릿의 다크 모드 테마이며, 우리 테마는 DayNight 라서 따로 필요 없습니다.
2. 메뉴 Build, Clean Project 를 실행한 뒤 File, Sync Project with Gradle Files 를 누릅니다.
3. 빨간 줄이 남아 있으면 7-13 의 체크리스트를 봅니다.

이 폴더의 Gradle 파일(`build.gradle`, `app/build.gradle` 등)은 "이 앱에 필요한 설정이 무엇인가"를 보여 주는 참고본입니다. 템플릿 Gradle 의 `namespace`, `applicationId` 가 `com.example.balancebot` 이고, `minSdk` 23, `compileSdk` 34 이상이며, 의존성에 appcompat 과 material 이 있는지만 비교해서 확인하면 됩니다.

## 7-03 Android 프로젝트 구조

Android 앱은 다음 다섯 가지 요소로 이루어집니다.

| 요소 | 이 프로젝트의 파일 | 역할 |
|---|---|---|
| Activity | `MainActivity.java` | 화면 하나를 담당하는 클래스. `onCreate()` 에서 레이아웃을 붙이고 버튼 동작을 연결합니다 |
| 레이아웃 XML | `res/layout/activity_main.xml` | 화면에 어떤 위젯을 어떤 순서로 놓을지 정의합니다. 각 위젯의 `android:id` 로 Java 에서 찾습니다 |
| 리소스 | `res/values/strings.xml`, `themes.xml` | 문자열, 색, 스타일을 코드 밖에 둡니다. 빌드하면 `R.string.xxx`, `R.style.xxx` 같은 정수 상수가 생깁니다 |
| 매니페스트 | `AndroidManifest.xml` | 앱 이름, 아이콘, 테마, 권한, 첫 화면(LAUNCHER Activity)을 시스템에 알립니다 |
| Gradle | `build.gradle`, `settings.gradle` 등 | 컴파일 SDK, 최소 SDK, 라이브러리 의존성, 빌드 방법을 정합니다 |

레이아웃과 Java 코드는 `R` 클래스로 이어집니다. 레이아웃에 `android:id="@+id/btnConnect"` 라고 쓰면 빌드할 때 `R.id.btnConnect` 상수가 만들어지고, Java 에서 `findViewById(R.id.btnConnect)` 로 그 버튼 객체를 얻습니다. id 철자가 하나라도 다르면 컴파일 오류가 나므로, 레이아웃을 고친 뒤에는 Java 쪽 id 도 같이 확인합니다.

이 앱의 Java 클래스 관계는 다음과 같습니다.

| 클래스 | 아는 것 | 모르는 것 |
|---|---|---|
| `Protocol` | 프레임 문자열, 체크섬, 숫자 변환 | Android, 블루투스, 화면 |
| `BluetoothService` | 소켓, 스레드, 바이트를 줄로 모으기 | 화면 위젯 |
| `JoystickView` | 터치 좌표, 그리기 | 블루투스, 프로토콜 형식 |
| `MainActivity` | 위 셋을 연결하고 화면을 갱신 | 소켓 세부 동작 |

`Protocol` 을 Android 와 분리해 둔 덕분에 휴대폰 없이 PC 에서 javac 로 테스트할 수 있습니다 (7-08).

## 7-04 ChatGPT로 레이아웃 설계하기

레이아웃 XML 은 반복이 많아 생성형 AI 의 도움을 받기 좋은 부분입니다. 다만 AI 가 만든 코드는 그럴듯해 보여도 id 가 다르거나, 쓰지 않기로 한 라이브러리를 끌어오는 일이 잦습니다. 그래서 **조건을 구체적으로 주고, 받은 결과를 정해진 순서로 검증**하는 습관이 중요합니다.

### 프롬프트 예

아래는 실제로 쓸 수 있는 프롬프트입니다. 7-01 의 와이어프레임 표를 그대로 붙여 넣는 것이 핵심입니다.

```
Android 앱 레이아웃 XML(activity_main.xml)을 만들어 주세요.

조건:
- 루트는 ScrollView, 그 안에 세로 LinearLayout 하나. ConstraintLayout 은 쓰지 마세요.
- 외부 라이브러리 금지. appcompat, material 만 사용합니다.
- 모든 문자열은 @string/ 리소스로 빼고, 필요한 strings.xml 항목도 함께 주세요.
- 아래 id 를 정확히 이 철자로 써 주세요. 다른 id 는 추가하지 마세요.
  tvConnState, btnConnect, btnDisconnect, switchMotor(androidx.appcompat.widget.SwitchCompat),
  tvAngle, tvOutput, tvBattery, tvState,
  joystick(커스텀 뷰 com.example.balancebot.JoystickView, 240dp 정사각형, 가운데 정렬), tvJoyValue,
  etKp, etKi, etKd, etTrim, etKs (inputType numberDecimal, etTrim 은 numberSigned 도),
  btnSendParams, btnReadParams, btnSaveParams, tvLastRx, tvRxStats
- 화면 구성(위에서 아래):
  (7-01 의 와이어프레임 표를 여기에 붙여 넣기)
- 같은 속성이 반복되는 구역 제목, 텔레메트리 값, 파라미터 라벨과 입력칸은 themes.xml 의 style 로 묶어 주세요.
```

### 받은 결과를 검증하는 법

AI 답변을 받으면 아래 순서로 확인합니다. 하나라도 걸리면 그 부분을 지적해서 다시 요청하거나 직접 고칩니다.

1. **루트와 금지 항목**: `ScrollView` 가 루트인지, `androidx.constraintlayout`, `app:layout_constraint` 같은 문자열이 없는지 찾기(Ctrl+F)로 확인합니다.
2. **id 대조**: 프롬프트에 준 id 목록과 XML 의 `@+id/` 를 하나씩 대조합니다. `btn_connect` 처럼 표기법을 멋대로 바꾸는 경우가 많습니다. 이 저장소 작업에서는 레이아웃의 `@+id` 목록과 Java 의 `R.id.` 목록을 스크립트로 뽑아 서로 빠진 것이 없는지 비교했습니다. 같은 일을 손으로 하려면 Android Studio 에서 Ctrl+Shift+F 로 `R.id.` 를 검색해 목록을 비교합니다.
3. **리소스 존재**: XML 의 `@string/xxx`, `@style/xxx` 가 모두 strings.xml, themes.xml 에 있는지 봅니다. 없는 리소스는 Android Studio 가 빨간색으로 표시합니다.
4. **위젯 종류**: Java 가 `SwitchCompat` 으로 받는데 XML 이 `Switch` 이면 실행 중 ClassCastException 으로 앱이 죽습니다. 커스텀 뷰는 패키지 이름까지 정확해야 합니다.
5. **Design 탭 미리보기**: 레이아웃 파일을 열고 오른쪽 위 Split 또는 Design 을 눌러 와이어프레임과 같은 순서로 보이는지 확인합니다.
6. **실행**: 마지막으로 휴대폰에서 실행해 스크롤, 키보드가 올라올 때 입력칸 가림 여부를 봅니다.

AI 가 레이아웃만 주고 Java 를 주지 않았다면, 레이아웃이 Java 와 맞는지 확인하는 책임은 우리에게 있다는 점을 기억하세요.

## 7-05 레이아웃 XML

전체 레이아웃입니다. 구역마다 제목 TextView 를 두고, 가로로 나란히 놓을 위젯은 가로 LinearLayout 에 `layout_width="0dp"`, `layout_weight="1"` 을 주어 같은 너비로 나눕니다.

<!-- include: part07_app/app/src/main/res/layout/activity_main.xml -->

눈여겨볼 점은 다음과 같습니다.

- `style="@style/SectionTitle"` 처럼 반복되는 속성은 themes.xml 의 스타일로 묶었습니다. 스타일 안에 `layout_width`, `layout_height` 를 넣었기 때문에 위젯 쪽에서는 생략해도 됩니다.
- `com.example.balancebot.JoystickView` 는 우리가 만든 커스텀 뷰라서 패키지 이름을 포함한 전체 이름으로 씁니다.
- `etTrim` 은 음수를 입력해야 하므로 `numberDecimal|numberSigned` 입니다. 나머지 게인은 0 이상이라 `numberDecimal` 만 씁니다.
- 앱 제목은 `NoActionBar` 테마라서 상단 바 대신 TextView 로 직접 보여 줍니다.

문자열과 테마 파일입니다.

<!-- include: part07_app/app/src/main/res/values/strings.xml -->

<!-- include: part07_app/app/src/main/res/values/themes.xml -->

`%1$s`, `%1$d` 는 Java 에서 `getString(R.string.tele_angle, "-1.25")` 처럼 값을 끼워 넣는 자리입니다. `$s` 는 문자열, `$d` 는 정수입니다.

