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

## 2-03 연산자

| 종류 | 연산자 | 기억할 점 |
|---|---|---|
| 산술 | `+ - * / %` | 정수 `/` 는 0 쪽으로 버림, `%` 의 부호는 왼쪽 피연산자를 따름 |
| 대입 | `= += -= *= /= %=` | `x += 5` 는 `x = x + 5` |
| 증감 | `++ --` | `i++` 는 사용 후 증가, `++i` 는 증가 후 사용 |
| 비교 | `== != < > <= >=` | 결과는 항상 0 또는 1 |
| 논리 | `&& \|\| !` | 단락 평가: 왼쪽만으로 결과가 정해지면 오른쪽은 실행하지 않음 |
| 조건 | `a ? b : c` | 값의 제한(clamp) 같은 짧은 선택에 사용 |

`uint8_t` 끼리 더해도 계산은 `int` 로 승격되어 이뤄지고, 저장할 때 잘립니다. 부호 있는 값과 부호 없는 값을 비교하면 부호 있는 값이 부호 없는 값으로 바뀌어 -1이 아주 큰 수가 됩니다.

<!-- include: part02_c/03_operators.c -->

실행 결과:

```
== 1. arithmetic ==
a + b = 22, a - b = 12, a * b = 85
a / b = 3, a % b = 2
-17 / 5 = -3, -17 % 5 = -2

== 2. compound assignment ==
x += 5  -> 15
x *= 2  -> 30
x -= 3  -> 27
x /= 4  -> 6
x %= 4  -> 2

== 3. prefix vs postfix ==
r1 = i++ : r1=5, i=6
r2 = ++i : r2=7, i=7

== 4. comparison and logical ==
t > 20 && t < 30 : 1
t < 0 || t > 100 : 0
!(t == 25)       : 0

== 5. short circuit ==
check(A,0) && check(B,1):
  check(A) called -> 0
check(A,1) || check(B,0):
  check(A) called -> 1

== 6. conditional (ternary) ==
clamp(-300) = -255

== 7. signed / unsigned comparison trap ==
(int)u > s            : 1 (expected)
u > (unsigned int)s   : 0 (surprise!)

== 8. integer promotion with uint8_t ==
p + q as int   = 300
p + q as uint8 = 44

== 9. precedence ==
2 + 3 * 4 = 14, (2 + 3) * 4 = 20
(flags & 0x04) == 0x04 : 1
```

자주 하는 실수:

- 한 식에서 같은 변수를 두 번 바꾸는 경우 (`i = i++ + 1;`). 정의되지 않은 동작입니다.
- `if (x & 0x04 == 0x04)` 처럼 비트 연산과 비교를 괄호 없이 섞는 경우.
- 부호 있는 값과 `sizeof`, `strlen` 결과(부호 없음)를 그대로 비교하는 경우.

연습 문제 힌트:

1. `k++ * 2` 는 증가 전의 값 3을 쓰므로 z = 6, 그 뒤 k = 4입니다.
2. `ok = (mv >= 6400) && (mv <= 8500);`
3. `comparison of integer expressions of different signedness` 경고가 나옵니다. -1이 4294967295로 바뀌어 비교된다는 뜻입니다.

## 2-04 비트 연산

임베디드 C에서 가장 많이 쓰는 문법입니다. 레지스터의 각 비트가 기능 하나를 켜고 끄기 때문입니다.

| 작업 | 식 | 예 (n = 2) |
|---|---|---|
| set (1로) | `reg \|= (1U << n)` | `0000_0000` 이 `0000_0100` |
| clear (0으로) | `reg &= ~(1U << n)` | `1111_1111` 이 `1111_1011` |
| toggle (반전) | `reg ^= (1U << n)` | `0000_0100` 이 `0000_0000` |
| test (읽기) | `(reg >> n) & 1U` | 해당 비트가 1이면 1 |

여러 비트로 된 필드를 바꿀 때는 "마스크로 지우고, 시프트한 새 값을 OR" 합니다 (읽기, 수정, 쓰기). 예제에서는 `DDRB`, `PORTB`, `PIND` 를 같은 이름의 전역 변수로 만들어 LED(PB2)와 모터 방향 핀, 풀업 버튼(PD3, 누르면 0)을 흉내 냅니다. Part 3에서는 이 코드가 거의 그대로 실제 레지스터에 쓰입니다.

<!-- include: part02_c/04_bitwise.c -->

실행 결과:

```
== 1. basic bit operators ==
a                      0b1100_1010  (0xCA, 202)
b                      0b0000_1111  (0x0F,  15)
a & b  (AND)           0b0000_1010  (0x0A,  10)
a | b  (OR)            0b1100_1111  (0xCF, 207)
a ^ b  (XOR)           0b1100_0101  (0xC5, 197)
~a     (NOT)           0b0011_0101  (0x35,  53)
a << 2                 0b0010_1000  (0x28,  40)
a >> 2                 0b0011_0010  (0x32,  50)

== 2. set / clear / toggle / test ==
SET_BIT(r,3)           0b0000_1000  (0x08,   8)
SET_BIT(r,0)           0b0000_1001  (0x09,   9)
TOGGLE_BIT(r,7)        0b1000_1001  (0x89, 137)
CLEAR_BIT(r,3)         0b1000_0001  (0x81, 129)
TEST_BIT(r,7) = 1, TEST_BIT(r,3) = 0

== 3. mask: read-modify-write a field ==
ctrl before            0b0011_1011  (0x3B,  59)
ctrl after (mode=5)    0b1011_1011  (0xBB, 187)
read mode = 5

== 4. register-like usage (fake DDRB / PORTB) ==
DDRB (PB0..PB2 out)    0b0000_0111  (0x07,   7)
PORTB LED on           0b0000_0100  (0x04,   4)
PORTB IN2=1, IN4=0     0b0000_0101  (0x05,   5)
blink 0: LED=0
blink 1: LED=1
blink 2: LED=0
button pressed? no
button pressed? yes

== 5. combine / split 16-bit with shifts ==
hi=0xFC lo=0x18 -> 0xFC18 = -1000
split back: hi=0xFC lo=0x18

== 6. precedence trap ==
status & 0x04 == 0x04   -> 0 (wrong)
(status & 0x04) == 0x04 -> 1 (right)
```

자주 하는 실수:

- `&&` 와 `&`, `||` 와 `|` 를 혼동하는 경우. `&&` 는 참과 거짓만, `&` 는 비트 단위로 계산합니다.
- `reg = (1 << n);` 처럼 `=` 를 써서 다른 비트를 모두 지워 버리는 경우. 다른 비트를 지키려면 `|=`, `&=` 를 씁니다.
- `1 << 31` 처럼 부호 있는 `int` 를 부호 비트까지 시프트하는 경우. `1U` 또는 `1UL` 을 씁니다. AVR에서는 `int` 가 16비트이므로 `1 << 15` 부터 문제가 됩니다.
- `==` 가 `&` 보다 우선순위가 높다는 것을 잊는 경우 (예제 6번).

연습 문제 힌트:

1. `while (v) { count += v & 1U; v >>= 1; }` 또는 `v &= (v - 1)` 로 가장 낮은 1 비트를 하나씩 지우며 셉니다.
2. `PORTB ^= (uint8_t)(BIT(PB0) | BIT(PB1));`
3. 2의 거듭제곱은 1 비트가 하나뿐입니다 (`0b1000`). 1을 빼면 그 아래가 모두 1이 되어 (`0b0111`) AND 결과가 0이 됩니다.

## 2-05 조건문 if와 switch

`if, else if, else` 는 위에서부터 차례로 검사해 처음 참인 블록 하나만 실행합니다. 범위를 나누는 판단에서는 조건의 순서가 결과를 바꿉니다.

`switch` 는 정수 값(문자 포함) 하나로 여러 갈래를 나눌 때 씁니다. 블루투스 명령 문자 `'D'`, `'P'`, `'S'` 처리가 대표적입니다. `break` 가 없으면 다음 `case` 로 계속 내려가며 (fall through), 의도한 경우에는 `/* fall through */` 주석을 남깁니다.

<!-- include: part02_c/05_if_switch.c -->

실행 결과:

```
== 1. if / else if / else ==
8350 mV -> FULL
7600 mV -> GOOD
7000 mV -> LOW
6500 mV -> CRITICAL

== 2. nested condition: is robot fallen? ==
angle -4200 -> WARNING (close to falling)

== 3. switch on command character ==
'D' -> joystick
'P' -> PID gains
'S' -> save to EEPROM
's' -> save to EEPROM
'G' -> get parameters
'X' -> unknown command (#E,4)

== 4. fall through ==
n=1 : one two three
n=2 : two three
n=3 : three
n=4 : other

== 5. = vs == ==
mode is not 1 (mode=0)
```

자주 하는 실수:

- `if (mode = 1)` 처럼 비교 대신 대입을 쓰는 경우. 항상 참이 되며, `-Wall` 이 경고합니다.
- `break` 를 빠뜨려 의도하지 않은 fall through가 생기는 경우.
- 중괄호 없이 `if` 아래에 두 문장을 쓰고 둘 다 조건에 걸린다고 생각하는 경우. 한 줄이라도 중괄호를 쓰는 습관이 안전합니다.

연습 문제 힌트:

1. `mv >= 6800` 이 먼저 참이 되므로 8350 mV도 "LOW" 가 됩니다. 범위 판단은 큰 값부터 (또는 작은 값부터) 일관되게 검사합니다.
2. `case 'V': printf(...); break;` 를 `default` 앞에 추가합니다.
3. `return (iabs(angle) > 4500) ? 4 : 3;`

## 2-06 반복문

| 문법 | 쓰임 |
|---|---|
| `for (초기; 조건; 증감)` | 반복 횟수가 정해져 있을 때 |
| `while (조건)` | 조건이 바뀔 때까지 |
| `do { } while (조건);` | 최소 한 번은 실행해야 할 때 (상태 확인 후 재시도) |
| `break` / `continue` | 반복 탈출 / 이번 반복만 건너뛰기 |
| `for (;;)` | 무한 루프, 펌웨어 `main` 의 슈퍼 루프 |

펌웨어의 `main` 은 초기화 뒤 `for (;;)` 안에서 "제어 주기가 됐는지 확인하고, 통신을 처리하는" 일을 끝없이 반복합니다. 이를 슈퍼 루프라고 부릅니다.

<!-- include: part02_c/06_loops.c -->

실행 결과:

```
== 1. for: fixed count ==
0 1 2 3 4 

== 2. while: until condition changes ==
1000 halved 9 times -> 1

== 3. do while: run at least once ==
poll #1 ready=0
poll #2 ready=0
poll #3 ready=1

== 4. break and continue ==
skip index 1
skip index 4
stop at index 5
sum = 49

== 5. nested loop ==
  1  2  3  4
  2  4  6  8
  3  6  9 12

== 6. super loop (embedded main) ==
tick 5: control step
tick 10: control step

== 7. infinite loop trap ==
loop ran 256 times
4 3 2 1 0 
```

자주 하는 실수:

- `for (uint8_t i = 0; i < 256; i++)`: `uint8_t` 는 256이 될 수 없어 끝나지 않습니다.
- `for (unsigned i = 4; i >= 0; i--)`: 부호 없는 값은 항상 0 이상이므로 끝나지 않습니다.
- `for (...);` 처럼 끝에 세미콜론을 붙여 빈 반복을 만드는 경우.

연습 문제 힌트:

1. `if (i % 3 != 0 || i % 5 == 0) continue; sum += i;` 정답은 1368입니다.
2. `do { digits++; n /= 10; } while (n != 0);` 를 쓰면 n = 0일 때도 1이 나옵니다.
3. tick 15에서 두 작업이 겹칩니다 (3과 5의 공배수). 실제 펌웨어도 같은 틱에 여러 작업이 몰릴 수 있음을 고려해야 합니다.

## 2-07 함수와 스택

함수를 `main` 보다 아래에 정의하면 위쪽에 원형(선언)을 둡니다. C의 인자 전달은 항상 값 전달이라서 함수 안에서 매개변수를 바꿔도 호출한 쪽 변수는 그대로입니다. 원본을 바꾸려면 주소(포인터)를 넘깁니다 (2-10).

함수를 호출할 때마다 복귀 주소, 매개변수, 지역 변수가 스택에 쌓이고 함수가 끝나면 사라집니다. ATmega328P의 SRAM은 2 KB뿐이므로 재귀나 큰 지역 배열은 스택 오버플로를 일으키기 쉽습니다. 펌웨어에서는 재귀를 쓰지 않습니다.

<!-- include: part02_c/07_functions.c -->

실행 결과 (4번의 바이트 수는 컴파일러, 최적화 옵션, 플랫폼에 따라 다르며 "호출마다 일정하게 늘어난다" 는 점만 보면 됩니다):

```
== 1. small reusable functions ==
clamp16(300, -255, 255)  = 255
clamp16(-80, -255, 255)  = -80
map_range(50, -100, 100, -255, 255) = 127
map_range(512, 0, 1023, 0, 5000)    = 2502

== 2. pass by value ==
before call: speed = 100
inside func: value = 0
after call : speed = 100 (unchanged)

== 3. recursion ==
0! = 1
5! = 120
10! = 3628800

== 4. stack grows with each call ==
depth 1: local is about 79 bytes below main's variable
depth 2: local is about 175 bytes below main's variable
depth 3: local is about 271 bytes below main's variable
depth 4: local is about 367 bytes below main's variable
```

자주 하는 실수:

- 함수 원형 없이 아래쪽 함수를 호출하는 경우 (C99에서는 경고, 결과를 신뢰할 수 없음).
- 매개변수를 바꾸면 원본이 바뀐다고 기대하는 경우.
- 반환형이 있는 함수에서 일부 경로에 `return` 이 없는 경우 (`-Wall` 이 `control reaches end of non-void function` 으로 알려 줌).

연습 문제 힌트:

1. `unsigned long r = 1; for (unsigned i = 2; i <= n; i++) r *= i;` 이 PC(`unsigned long` 64비트)에서는 21!부터 넘칩니다. AVR(32비트)에서는 13!부터 넘칩니다.
2. 나눗셈을 먼저 하면 `(50 + 100) / 200` 이 0이 되어 결과가 크게 틀립니다. 정수 연산은 곱셈을 먼저 합니다 (단, 곱셈 결과가 넘치지 않는지 확인).
3. `(int16_t)(((int32_t)a + b) / 2)` 처럼 넓은 자료형에서 더합니다.

## 2-08 배열과 이동평균 필터

배열은 같은 자료형을 연속된 메모리에 나란히 둔 것입니다. 인덱스는 0부터 `길이 - 1` 까지이며, C는 범위를 검사하지 않습니다. 요소 개수는 `sizeof(a) / sizeof(a[0])` 로 구합니다 (배열 이름에서만 동작).

이동평균 필터는 최근 N개 샘플의 평균입니다. 매번 N개를 다시 더하지 않고 "가장 오래된 값을 빼고 새 값을 더하는" 누적 합을 쓰면 샘플당 연산이 일정합니다. N이 클수록 부드럽지만 반응이 늦어지므로 배터리 전압처럼 느리게 바뀌는 값에 씁니다.

<!-- include: part02_c/08_arrays.c -->

실행 결과:

```
== 1. declare and initialize ==
a[0] = 10
a[1] = 20
a[2] = 30
a[3] = 0
a[4] = 0
sizeof(a) = 20 bytes, length = 5

== 2. min / max / average ==
min=-20 max=30 avg=5

== 3. 2D array as lookup table ==
CALIB    led= 100 ms motor=0
BALANCE  led= 500 ms motor=1
FALLEN   led=1000 ms motor=0

== 4. moving average filter (N=4) ==
raw  :  500 510 490 505 900 495 500 508 492 500
filt :  500 505 500 501 601 597 600 600 498 500

== 5. out of bounds (do not do this) ==
index 7 rejected (length 5)
```

자주 하는 실수:

- `int a[5];` 에서 `a[5]` 에 접근하는 경우 (유효 인덱스는 0..4).
- 지역 배열을 초기화하지 않고 읽는 경우. 전역과 `static` 배열만 0으로 자동 초기화됩니다.
- 함수 매개변수로 받은 배열에 `ARRAY_LEN` 을 쓰는 경우 (2-11 참고).
- 누적 합을 작은 자료형으로 두어 넘치는 경우.

연습 문제 힌트:

1. N = 8이면 900의 영향이 약 1/8로 줄지만 8샘플 동안 남습니다. 대신 처음 값이 안정되는 데 더 오래 걸립니다.
2. `for (i = 0, j = n - 1; i < j; i++, j--) { tmp = a[i]; a[i] = a[j]; a[j] = tmp; }`
3. 세 값 중 최댓값도 최솟값도 아닌 값을 고르면 중앙값입니다. 한 번 튀는 값은 중앙값이 될 수 없어 완전히 사라집니다.

