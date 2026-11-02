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

