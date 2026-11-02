# Part 2. 임베디드 Basic C언어

이 파트에서는 PC에서 C 언어의 기초를 익힙니다. 목표는 문법 자체보다, 이후 Part 3의 ATmega328P 레지스터 프로그래밍과 Part 8의 밸런싱 로봇 펌웨어 코드를 막힘 없이 읽고 고칠 수 있게 되는 것입니다. 그래서 예제는 가능한 한 로봇에서 실제로 쓰일 모양(레지스터 비트 조작, MPU6050 바이트 결합, 상태 머신, 명령 프레임 파싱, UART 수신 링버퍼)으로 만들었습니다.

## 파트 소개: 왜 임베디드에서 C인가

| 이유 | 설명 |
|---|---|
| 하드웨어에 가까움 | 포인터와 비트 연산으로 레지스터(정해진 메모리 주소)를 직접 읽고 씁니다. `PORTB \|= (1 << PB2);` 한 줄이 핀 하나를 High로 만듭니다. |
| 작고 빠른 결과물 | ATmega328P는 플래시 32 KB, SRAM 2 KB입니다. C는 런타임이 거의 없어 이 안에 충분히 들어갑니다. |
| 예측 가능한 동작 | 가비지 컬렉션 같은 숨은 동작이 없어 5 ms 제어 주기를 지키기 쉽습니다. |
| 사실상의 표준 | 제조사 헤더(`avr/io.h`), 데이터시트 예제, 대부분의 펌웨어가 C로 되어 있습니다. |

반대로 C는 배열 범위 검사, 넘침 검사를 해 주지 않습니다. 이 파트의 예제마다 "자주 하는 실수" 를 함께 둔 이유입니다.

이 파트의 모든 실습은 PC에서 실행합니다. AVR에서만 있는 것(레지스터, 인터럽트)은 같은 이름의 변수와 함수로 흉내 내고, 실제 AVR 코드 모양은 주석으로 보여 줍니다.

### 파일 구성

```
part02_c/
  README.md                 이 문서
  Makefile                  make, make run, make clean
  01_hello.c ... 17_function_pointer.c
  18_ringbuffer/
    ringbuf.h  ringbuf.c  main.c
```

모든 파일은 상단 주석에 학습 목표, 본문 주석에 설명, 하단 주석에 연습 문제가 있습니다. 이 문서는 각 파일의 핵심을 요약하고 실제 실행 결과를 보여 줍니다. 코드는 반드시 직접 빌드해서 출력과 비교해 보세요.

## 개발환경 준비

### 방법 1: Dev-C++ (Windows)

1. Embarcadero Dev-C++ (또는 Orwell Dev-C++ 5.11)을 설치합니다. 설치 프로그램에 TDM-GCC 컴파일러가 함께 들어 있으므로 따로 설치할 것이 없습니다.
2. 처음 실행하면 언어와 테마를 고르는 화면이 나옵니다. 원하는 대로 고르고 넘어갑니다.
3. C99 문법을 쓰도록 설정합니다. 메뉴 `도구(Tools) > 컴파일러 설정(Compiler Options)` 에서 "컴파일러를 호출할 때 다음 명령 추가" 를 체크하고 `-std=c99 -Wall -Wextra` 를 입력합니다.
4. 새 소스 파일 만들기: `파일(File) > 새로 만들기(New) > 소스 파일(Source File)` (단축키 Ctrl+N).
5. 이 폴더의 `01_hello.c` 내용을 붙여 넣거나, `파일 > 열기` 로 파일을 직접 엽니다.
6. 저장할 때 확장자를 반드시 `.c` 로 합니다. `.cpp` 로 저장하면 C++ 컴파일러로 빌드되어 일부 예제(지정 초기화 등)에서 오류가 납니다.
7. `실행(Execute) > 컴파일 후 실행(Compile & Run)` (F11)을 누르면 콘솔 창에 결과가 나옵니다.
8. 18_ringbuffer처럼 파일이 여러 개인 경우 `파일 > 새로 만들기 > 프로젝트 > Console Application` 에서 "C 프로젝트" 를 고르고, 자동으로 생긴 main.c를 지운 뒤 `프로젝트 > 프로젝트에 추가` 로 세 파일을 넣고 빌드합니다.

Windows 콘솔에서 한글 주석은 출력과 무관하므로 문제가 없습니다. 이 파트의 출력은 모두 영문으로 만들어 콘솔 인코딩 문제를 피했습니다.

### 방법 2: gcc 명령줄 (Linux, macOS, Windows의 MSYS2 또는 WSL)

```
gcc --version                               # 설치 확인
gcc -Wall -Wextra -std=c99 04_bitwise.c -o 04_bitwise
./04_bitwise
```

| 옵션 | 의미 |
|---|---|
| `-Wall -Wextra` | 흔한 실수에 대한 경고를 최대한 켭니다. 이 파트의 모든 코드는 경고 0개입니다. |
| `-std=c99` | C99 표준으로 컴파일합니다 (`for (int i ...)`, `//` 주석, `stdint.h`, 지정 초기화). avr-gcc에서도 같은 옵션을 씁니다. |
| `-o 이름` | 만들 실행 파일 이름입니다. |

### Makefile로 한 번에

```
cd part02_c
make          # build/ 폴더에 18개 실행 파일 생성
make run      # 18개를 차례로 실행
make clean    # build/ 삭제
make WERROR=1 # 경고를 오류로 취급해 경고 0개인지 확인
```

이 문서의 실행 결과는 Ubuntu 24.04, gcc 13.3.0 (x86-64)에서 `make run` 으로 실제 실행한 출력입니다. `sizeof` 나 주소 차이처럼 플랫폼에 따라 달라지는 값은 해당 레슨에 따로 표시했습니다.

## 2-01 Hello World와 컴파일 과정

C 소스는 네 단계를 거쳐 실행 파일이 됩니다.

| 단계 | gcc 옵션 | 결과물 | 하는 일 |
|---|---|---|---|
| 전처리 | `-E` | `.i` | `#include` 내용을 붙여 넣고, `#define` 을 치환하고, 주석을 지움 |
| 컴파일 | `-S` | `.s` | C를 그 CPU의 어셈블리어로 번역, 문법 오류와 경고가 여기서 나옴 |
| 어셈블 | `-c` | `.o` | 어셈블리를 기계어 오브젝트 파일로 |
| 링크 | (없음) | 실행 파일 | 여러 `.o` 와 라이브러리를 합쳐 `printf` 같은 함수 주소를 채움 |

`gcc -save-temps 01_hello.c` 로 빌드하면 중간 파일이 모두 남습니다. AVR에서는 같은 과정을 `avr-gcc -mmcu=atmega328p` 로 진행하고, 마지막에 `.elf` 를 `.hex` 로 바꿔 AVRISP mkII로 기록합니다. PC용 실행 파일과 AVR용 hex 파일은 CPU가 달라 서로 호환되지 않습니다 (크로스 컴파일).

운영체제가 없는 AVR에서 `main` 은 끝나면 돌아갈 곳이 없으므로 항상 무한 루프로 끝납니다.

<!-- include: part02_c/01_hello.c -->

실행 결과:

```
Hello, Embedded World!
This course has 10 parts. Now: Part 2 (C basics)
Build stages: preprocess(.i) -> compile(.s) -> assemble(.o) -> link(exe)
__STDC_VERSION__ = 199901
```

자주 하는 실수:

- 파일을 `.cpp` 로 저장해 C++로 컴파일되는 경우
- `#include <stdio.h>` 를 빠뜨려 `implicit declaration of function 'printf'` 경고가 나는 경우
- "undefined reference" (링크 오류)를 문법 오류로 착각하는 경우. 이 오류는 선언은 있지만 정의(구현)를 찾지 못했다는 뜻입니다.

연습 문제 힌트:

1. `.i` 파일은 `stdio.h` 와 그 안에서 다시 포함한 헤더들이 모두 펼쳐져 수백 줄이 됩니다 (`wc -l 01_hello.i`).
2. `prinft` 는 컴파일 단계에서 "implicit declaration" 경고, 링크 단계에서 "undefined reference to prinft" 오류가 납니다. 두 단계가 다르다는 것이 핵심입니다.
3. `printf("%s %d-%d-%d\n", "홍길동", 2026, 11, 2);` 처럼 변환 지정자 개수와 인자 개수를 맞춥니다.

## 2-02 자료형과 고정폭 정수

`int` 의 크기는 정해져 있지 않습니다. PC gcc에서는 4바이트, avr-gcc에서는 2바이트입니다. 그래서 크기가 의미 있는 값에는 `stdint.h` 의 고정폭 정수를 씁니다.

| 자료형 | 범위 | 로봇에서의 예 |
|---|---|---|
| `uint8_t` | 0..255 | 레지스터 값, UART 바이트 |
| `int16_t` | -32768..32767 | MPU6050 원시값 |
| `uint16_t` | 0..65535 | ADC 결과, 배터리 mV |
| `int32_t` | 약 ±21억 | 곱셈 중간값, PID 적분 |
| `uint32_t` | 0..약 42억 | 밀리초 카운터 |

부호 없는 정수는 최댓값 다음에 0으로 돌아옵니다 (`uint8_t 255 + 1 = 0`). 작은 자료형으로 바꾸면 상위 비트가 잘립니다 (`(uint8_t)300 = 44`). AVR에는 부동소수점 하드웨어가 없으므로 블루투스 프로토콜처럼 "실수는 100배 정수" 로 다룹니다.

<!-- include: part02_c/02_types.c -->

실행 결과 (sizeof 값은 PC 기준이며, AVR에서는 int 2, long 4, double 4입니다):

```
== 1. sizeof (bytes on this PC) ==
char      : 1
short     : 2
int       : 4
long      : 8
float     : 4
double    : 8
uint8_t   : 1
int16_t   : 2
uint32_t  : 4

== 2. unsigned wrap around ==
250 251 252 253 254 255 0 1 
uint8_t 0 - 1 = 255

== 3. truncation when storing into a smaller type ==
(uint8_t)300 = 44
(int16_t)40000 = -25536

== 4. 16-bit int trap (AVR style) ==
int16 result : -31072
int32 result : 100000

== 5. integer vs floating division ==
7 / 2         = 3
7 % 2         = 1
7 / 2.0       = 3.50
(float)7 / 2  = 3.50

== 6. fixed point instead of float ==
angle_x100 = -1234 -> -12.34 deg

== 7. printf formats for fixed width ==
millis = 4000000000
0xAB 0x1234
```

자주 하는 실수:

- `int16_t` 끼리 곱한 결과가 16비트를 넘는데 그대로 16비트 변수에 담는 경우. 곱하기 전에 `(int32_t)` 로 한쪽을 넓힙니다.
- `7 / 2` 가 3.5라고 기대하는 경우. 정수끼리의 나눗셈은 소수점을 버립니다.
- `uint32_t` 를 `%d` 로 출력하는 경우. `inttypes.h` 의 `PRIu32` 를 쓰거나 `(unsigned long)` 으로 바꿔 `%lu` 로 출력합니다.

연습 문제 힌트:

1. 127 + 1 = 128은 `int8_t` 범위를 넘으므로 2의 보수로 잘려 -128이 됩니다.
2. `adc * 5000` 은 최대 5,115,000이라 `uint16_t` 에 들어가지 않습니다. `(uint32_t)adc * 5000 / 1023` 처럼 넓혀서 계산합니다.
3. 부호를 먼저 따로 출력합니다. 예: `if (v < 0) { putchar('-'); v = -v; }` 다음에 `v / 100`, `v % 100` 을 출력합니다.

