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

## 2-09 문자열과 파싱

C 문자열은 끝에 `'\0'` (값 0)이 붙은 `char` 배열입니다. `"HC-05"` 는 5글자지만 6바이트를 차지합니다. 문자열은 `==` 로 비교하면 주소를 비교하므로 내용 비교에는 `strcmp` 를 씁니다.

통신 데이터는 깨질 수 있으므로 `atoi` 대신 오류를 구분하는 파서를 직접 만듭니다. 핵심은 `'7' - '0' = 7` 이라는 것과 `value = value * 10 + digit` 입니다. 반대로 숫자를 문자열로 만들 때는 버퍼 크기를 받는 `snprintf` 를 씁니다. `sprintf` 는 버퍼가 넘쳐도 멈추지 않습니다.

<!-- include: part02_c/09_strings.c -->

실행 결과:

```
== 1. a string is a char array ending with '\0' ==
text="HC-05" strlen=5 sizeof=6
name[0] = 0x48 'H'
name[1] = 0x43 'C'
name[2] = 0x2D '-'
name[3] = 0x30 '0'
name[4] = 0x35 '5'
name[5] = 0x00 (end mark)

== 2. compare and copy ==
strcmp(cmd, "BALANCE") = 0
strcmp("A", "B")       = -1 (negative)
copied into 6-byte buffer: "WAIT_"

== 3. parse integers by hand ==
"1200" -> OK 1200
"-350" -> OK -350
"+42" -> OK 42
"0" -> OK 0
"abc" -> ERROR
"" -> ERROR
"99999999999" -> ERROR
"-2147483648" -> OK -2147483648

== 4. parse comma separated fields ==
kp=1200 ki=50 kd=80

== 5. build a frame with snprintf ==
frame="#A,-152,87,7820,3" length=17
tiny="#A,-152" wanted=10 (truncated: yes)

== 6. hex digits ==
0x47 as text: "47"
```

자주 하는 실수:

- `'\0'` 자리를 계산하지 않아 버퍼를 한 칸 작게 잡는 경우.
- `strncpy` 가 항상 `'\0'` 을 붙여 준다고 생각하는 경우 (원본이 길면 붙이지 않음).
- `'A'` (문자, 정수 65)와 `"A"` (문자열, 2바이트 배열)를 혼동하는 경우.
- `atoi` 의 반환값 0을 "숫자 0" 으로 믿는 경우.

연습 문제 힌트:

1. `if (c >= '0' && c <= '9') return c - '0'; if (c >= 'A' && c <= 'F') return c - 'A' + 10;` 소문자도 같은 방식으로 처리하고 나머지는 -1을 돌려줍니다.
2. `"P,1200,,80"` 은 두 번째 숫자에서 숫자가 없어 실패, `"P,1200,50,80,7"` 은 세 번째 숫자 뒤가 `'\0'` 이 아니어서 형식 오류가 납니다.
3. 음수면 부호를 기억하고 양수로 바꾼 뒤 `n % 10 + '0'` 을 배열에 쌓고, 마지막에 배열을 뒤집고 `'\0'` 을 붙입니다.

## 2-10 포인터 기초

포인터는 주소를 담는 변수입니다. `&x` 는 x의 주소, `*p` 는 p가 가리키는 곳의 값입니다.

```
int16_t speed = 120;
int16_t *p = &speed;   // p 에는 speed 의 주소
*p = 200;              // speed 가 200 이 됨
```

함수가 원본을 바꾸거나 결과를 여러 개 돌려줘야 할 때 포인터 매개변수를 씁니다. `const int16_t *p` 는 "가리키는 값을 바꾸지 않겠다" 는 약속입니다. AVR의 `PORTB` 도 사실은 `(*(volatile uint8_t *)0x25)` 라는 포인터 역참조 매크로입니다.

<!-- include: part02_c/10_pointers.c -->

실행 결과 (포인터 크기는 PC에서 8바이트, AVR에서 2바이트):

```
== 1. address and dereference ==
speed       = 120
*ptr        = 120 (value at the address)
ptr == &speed ? yes
sizeof(ptr) = 8 bytes on this PC (2 bytes on AVR)
after *ptr = 200 -> speed = 200

== 2. swap through pointers ==
before: left=100 right=-50
after : left=-50 right=100

== 3. multiple outputs ==
7820 mV = 7.820 V
divide by zero rejected

== 4. NULL and const ==
trim: -35
none: (null)

== 5. pointer to pointer (just a glance) ==
left=77

== 6. registers are memory addresses ==
fake_io[0x25] = 0x05

== 7. dangling pointer (concept) ==
never return the address of a local variable
```

자주 하는 실수:

- 초기화하지 않은 포인터를 역참조하는 경우.
- `NULL` 검사 없이 매개변수 포인터를 쓰는 경우.
- 함수의 지역 변수 주소를 반환하는 경우 (댕글링 포인터).
- `int16_t* a, b;` 에서 b도 포인터라고 생각하는 경우. `*` 는 a에만 붙습니다.

연습 문제 힌트:

1. `if (*v > 255) *v = 255; else if (*v < -255) *v = -255;`
2. 첫 요소로 `*mn`, `*mx` 를 초기화한 뒤 나머지를 비교합니다. `n <= 0` 이면 아무것도 하지 않도록 검사합니다.
3. `#define FAKE_DDRB (*(volatile uint8_t *)&fake_io[0x24])` 후 `FAKE_DDRB |= (1U << 2);` 결과는 0x04입니다.

## 2-11 포인터와 배열, 버퍼 처리

배열 이름은 대부분의 식에서 첫 요소의 주소로 바뀝니다. 그래서 `a[i]` 는 `*(a + i)` 와 같습니다. 포인터에 1을 더하면 주소가 1바이트가 아니라 `sizeof(요소)` 만큼 증가합니다.

함수에 배열을 넘기면 포인터만 넘어가므로 함수 안에서 `sizeof` 로 길이를 알 수 없습니다. 길이를 항상 함께 넘깁니다. 예제 5번에서는 UART로 받은 바이트 버퍼를 포인터로 훑으며 `'\n'` 단위로 줄을 잘라 내고, SPEC 4장의 XOR 체크섬을 검사합니다. 두 번째 줄은 일부러 틀린 체크섬을 넣었습니다.

<!-- include: part02_c/11_pointer_array.c -->

실행 결과 (`sizeof(a)` 의 8은 PC의 포인터 크기입니다):

```
== 1. array name decays to pointer ==
raw[2] = 30, *(p + 2) = 30, p[2] = 30

== 2. pointer arithmetic step = sizeof(element) ==
uint8_t : (p+1) - p = 1 bytes
int16_t : (p+1) - p = 2 bytes
uint32_t: (p+1) - p = 4 bytes
element distance b16+3 - b16 = 3 elements

== 3. array as function parameter ==
sizeof(raw) in main = 10
  inside sum16: sizeof(a) = 8 (pointer!)
sum = 150
scaled: 30 60 90 120 150 

== 4. sub-array by pointer offset ==
  inside sum16: sizeof(a) = 8 (pointer!)
sum of raw[2..4] = 360

== 5. byte buffer: split lines and verify checksum ==
"$G*47" -> calc=47 given=47 OK
"$M,1*51" -> calc=50 given=51 CHECKSUM ERROR (#E,1)
left in buffer (no newline yet): "$D,10" (5 bytes)
```

자주 하는 실수:

- 함수 안에서 `sizeof(배열 매개변수)` 로 길이를 구하는 경우.
- 끝 포인터(`p + n`)를 역참조하는 경우. 끝 포인터는 비교용으로만 씁니다.
- 서로 다른 배열을 가리키는 포인터끼리 빼거나 비교하는 경우.

연습 문제 힌트:

1. `const int16_t *end = a + n; while (a < end) s += *a++;`
2. `rx` 의 `$M,1*51` 을 `$M,1*50` 으로 바꿉니다.
3. `for (const uint8_t *p = buf; p < buf + n; p++) if (*p == c) return p; return NULL;`

## 2-12 구조체

구조체는 관련된 값을 하나로 묶은 사용자 정의 자료형입니다. `typedef` 로 이름을 붙이고, `.필드 = 값` 형태의 지정 초기화로 읽기 쉽게 초기화합니다. 구조체는 대입하면 통째로 복사되고, 포인터로 받을 때는 `p->field` (`(*p).field` 와 같음)로 접근합니다.

예제에서는 MPU6050 원시값(`ImuRaw`), PID 파라미터(`PidParam`), PID 상태(`PidState`)를 구조체로 만들었습니다. `PidState` 는 파라미터를 포인터로 가리키므로 파라미터를 바꾸면 바로 반영됩니다. 첫 출력이 -255로 포화되는 것은 첫 스텝에서 미분항이 크게 튀는 현상(derivative kick)이며, Part 8에서 다룹니다.

컴파일러는 멤버를 정렬하려고 빈칸(패딩)을 넣을 수 있어 `sizeof` 가 멤버 크기의 합보다 클 수 있습니다.

<!-- include: part02_c/12_struct.c -->

실행 결과 (패딩 결과는 PC 기준이며, AVR에서는 둘 다 6바이트입니다):

```
== 1. struct and designated initializer ==
acc(   120,   -40, 16300) gyro(    85,   -12,    30)
sizeof(ImuRaw) = 14 (7 x int16_t)

== 2. assignment copies all members ==
sample.gx=85 copy.gx=0

== 3. modify through pointer (->) ==
acc(   120,   -40, 16300) gyro(     5,    -2,     5)

== 4. array of structs ==
log[0] ax=   0 (x0.01 g)  gx=  100 (x0.01 dps)
log[1] ax=  50 (x0.01 g)  gx= -200 (x0.01 dps)
log[2] ax=  -1 (x0.01 g)  gx=    0 (x0.01 dps)

== 5. struct holding a pointer: PID ==
angle=  500 -> out=-255 (integral=-500)
angle=  300 -> out=   4 (integral=-800)
angle=  100 -> out=  54 (integral=-900)
angle=  -50 -> out=  72 (integral=-850)
angle= -100 -> out=  44 (integral=-750)
after kp change: out=49

== 6. padding ==
struct Padded: sizeof=12 offsets a=0 b=4 c=8
struct Packed: sizeof=8 offsets b=0 a=4 c=5
```

자주 하는 실수:

- 큰 구조체를 값으로 넘겨 매번 복사하는 경우. 읽기만 하면 `const` 포인터로 넘깁니다.
- 구조체를 `==` 로 비교하는 경우 (C에서는 불가능, 멤버별로 비교).
- 통신으로 받은 바이트를 구조체에 `memcpy` 해서 해석하는 경우. 패딩과 엔디언 때문에 플랫폼마다 결과가 다릅니다.

연습 문제 힌트:

1. `b->mv = mv; int p = ((int)mv - 6400) * 100 / 2000;` 그다음 0..100으로 제한하고, `b->low = (mv < 6400);`
2. 큰 멤버를 앞에 둡니다: `uint32_t b; uint8_t a; uint8_t c;` 는 4 + 1 + 1 = 6에 정렬 패딩 2를 더해 8입니다.
3. `if (s->integral > 100000) s->integral = 100000;` 처럼 누적 직후 양쪽을 제한합니다.

## 2-13 공용체, 엔디언, 비트필드

공용체(`union`)는 모든 멤버가 같은 메모리를 공유합니다. `uint16_t` 로 쓰고 `uint8_t[2]` 로 읽으면 엔디언(바이트 순서)을 직접 볼 수 있습니다. x86과 AVR은 모두 리틀엔디언(낮은 주소에 하위 바이트)입니다.

MPU6050은 상위 바이트를 먼저 보내는 빅엔디언입니다. 두 바이트는 공용체가 아니라 시프트와 OR로 합칩니다. 이렇게 하면 CPU 엔디언과 무관하게 항상 같은 결과가 나옵니다.

```
int16_t v = (int16_t)(((uint16_t)hi << 8) | lo);   // 0xFC, 0x18 -> -1000
```

비트필드는 구조체 멤버를 비트 단위로 나눕니다. 비트 배치 순서는 C 표준이 정하지 않으므로, 실제 레지스터 접근은 마스크와 시프트로 하고 비트필드는 개념 모델이나 내부 플래그 저장에 씁니다.

<!-- include: part02_c/13_union_bitfield.c -->

실행 결과:

```
== 1. union shares memory ==
sizeof(Split16)=2 sizeof(Split32)=4
0x1234 bytes:    [0]=0x34 [1]=0x12
this PC is LITTLE endian
0xDEADBEEF:      [0]=0xEF [1]=0xBE [2]=0xAD [3]=0xDE

== 2. build 16-bit from bytes via union ==
bytes {0x18,0xFC} as word = 0xFC18 (-1000)

== 3. MPU6050 big endian burst read ==
AX   H=0x00 L=0x78 ->    120
AY   H=0xFF L=0xD8 ->    -40
AZ   H=0x3F L=0xAC ->  16300
TEMP H=0xFB L=0x50 ->  -1200
GX   H=0x00 L=0x55 ->     85
GY   H=0xFF L=0xF4 ->    -12
GZ   H=0x00 L=0x1E ->     30
temperature = 33.01 C

== 4. bit-field register model ==
FS_SEL=0 -> reg=0x00
FS_SEL=3 -> reg=0x18
XG_ST=1  -> reg=0x98
mask way -> reg=0x98

== 5. bit-field flags ==
sizeof(RobotFlags)=1 motor=1 low=0 imu=1 bt=0 state=3
state=17 stored as 1
```

자주 하는 실수:

- 캐스트 없이 `(hi << 8) | lo` 로 합치는 경우. AVR(16비트 `int`)에서 `hi << 8` 이 부호 있는 `int` 범위를 넘을 수 있습니다.
- `int16_t` 로 바꾸지 않고 `uint16_t` 결과를 그대로 써서 음수가 65000대의 큰 양수로 나오는 경우.
- 상위, 하위 바이트 순서를 반대로 조합하는 경우.

연습 문제 힌트:

1. `p[0] = (uint8_t)((uint16_t)v >> 8); p[1] = (uint8_t)((uint16_t)v & 0xFF);`
2. `uint8_t` 는 `int` 로 승격된 뒤 시프트됩니다. 16비트 `int` 에서 `0xFC << 8` 은 부호 있는 범위(최대 32767)를 넘어 정의되지 않은 동작이 됩니다.
3. AFS_SEL = 2를 bit4..3에 넣으면 `2 << 3 = 0x10` 입니다.

## 2-14 enum과 상태 머신

`enum` 은 이름 붙은 정수 상수의 묶음입니다. 값을 명시하면 SPEC 4장의 상태 번호와 그대로 맞출 수 있고, 마지막에 `ST_COUNT` 를 두면 상태 개수를 배열 크기로 쓸 수 있습니다.

상태 머신은 "현재 상태" 변수 하나와, 매 주기 호출되는 `switch` 로 만듭니다. 각 상태는 자기 조건만 검사하고, 상태 변경은 한 함수(`change_state`)에서만 하도록 모아 로그와 진입 동작(entry action)을 처리합니다.

```
INIT -> CALIB -> WAIT_UPRIGHT -> BALANCE <-> FALLEN
                       (어느 상태든 배터리 저전압이면 LOW_BATT)
```

<!-- include: part02_c/14_enum_state_machine.c -->

실행 결과 (`sizeof(RobotState)` 는 PC gcc 기준이며, avr-gcc 기본 설정에서는 2입니다):

```
sizeof(RobotState) = 4, ST_COUNT = 7

t= 0 angle= 3000 mv=8200 [INIT]
        state INIT -> CALIB
t= 1 angle= 3000 mv=8200 [CALIB]
t= 2 angle= 3000 mv=8200 [CALIB]
t= 3 angle= 3000 mv=8190 [CALIB]
        gyro offset = 12
        state CALIB -> WAIT_UPRIGHT
t= 4 angle= 1200 mv=8190 [WAIT_UPRIGHT]
t= 5 angle=  300 mv=8180 [WAIT_UPRIGHT]
        state WAIT_UPRIGHT -> BALANCE
        (entry) reset PID integral, enable motors
t= 6 angle= -150 mv=8180 [BALANCE]
t= 7 angle= 2000 mv=8170 [BALANCE]
t= 8 angle= 5200 mv=8170 [BALANCE]
        state BALANCE -> FALLEN
        (entry) motors OFF
t= 9 angle= 6000 mv=8160 [FALLEN]
t=10 angle=  100 mv=8160 [FALLEN]
        state FALLEN -> BALANCE
        (entry) reset PID integral, enable motors
t=11 angle=   50 mv=6300 [BALANCE]
        state BALANCE -> LOW_BATT
        (entry) motors OFF
t=12 angle=   50 mv=6300 [LOW_BATT]

final state = LOW_BATT (telemetry st=6)
```

자주 하는 실수:

- 여러 곳에서 `state = ...` 를 직접 바꿔 흐름을 추적할 수 없게 되는 경우.
- 진입 시 한 번만 할 일(모터 정지, 적분 초기화)을 매 주기 반복하는 경우.
- `enum` 값과 이름 표의 순서가 어긋나는 경우. `ST_COUNT` 로 배열 크기를 고정하면 개수 차이를 컴파일러가 일부 잡아 줍니다.

연습 문제 힌트:

1. FALLEN에서 섰다는 조건이 거짓이면 별도 카운터를 0으로, 참이면 증가시키고 200 이상일 때 `change_state` 를 호출합니다.
2. `if (!enable) change_state(r, ST_IDLE); else if (r->state == ST_IDLE) change_state(r, ST_WAIT_UPRIGHT);`
3. `default` 가 있으면 `case ST_IDLE` 을 지워도 `default` 로 처리되므로 경고가 없습니다. `default` 까지 지우면 `-Wall` 에 포함된 `-Wswitch` 가 `enumeration value 'ST_IDLE' not handled in switch` 경고를 냅니다. 그래서 상태 머신에서는 일부러 `default` 없이 모든 `case` 를 나열해 빠진 상태를 컴파일러가 찾게 하는 방식도 많이 씁니다.

## 2-15 전처리기

전처리기는 컴파일 전에 글자를 치환합니다. 매크로 함수는 함수가 아니라 글자 치환이므로 다음 두 규칙을 지킵니다.

1. 인자 하나하나와 전체 식에 괄호를 씌웁니다: `#define SQUARE(x) ((x) * (x))`
2. 인자에 `++` 나 함수 호출을 넣지 않습니다. 두 번 평가될 수 있습니다. 가능하면 `static inline` 함수를 씁니다.

설정 매크로(`F_CPU`, `BAUD`, `MOTOR_R_INVERT`)는 한곳에 모으고, `#if` 와 `#error` 로 잘못된 값을 빌드 단계에서 막습니다. `#ifndef F_CPU` 처럼 감싸 두면 `-DF_CPU=8000000UL` 로 명령줄에서 바꿀 수 있습니다. 여러 문장 매크로는 `do { ... } while (0)` 로 감쌉니다.

<!-- include: part02_c/15_preprocessor.c -->

실행 결과:

```
== 1. config macros ==
F_CPU=16000000 BAUD=9600 UBRR_VALUE=207
control period = 5000 us

== 2. parentheses matter ==
SQUARE_BAD(a + 1)  = 7
SQUARE_GOOD(a + 1) = 16
10 * DOUBLE_BAD(a)  = 33
10 * DOUBLE_GOOD(a) = 60

== 3. double evaluation ==
MAX_MACRO(next_value(), 5) = 20, calls = 2
max_inline(next_value(), 5) = 10, calls = 1

== 4. conditional compile and do-while(0) ==
[15_preprocessor.c:108] a is positive
DEBUG_LOG is ON (build with -DDEBUG_LOG=0 to remove logs)

== 5. # and ## ==
STR(CONTROL_HZ)  = CONTROL_HZ
XSTR(CONTROL_HZ) = 200
PORTB=0x04 PORTD=0x80

== 6. motor invert config ==
cmd=120 -> left=120 right=-120

== 7. predefined macros ==
file=15_preprocessor.c line=130 func=main
target: PC (not __AVR__)
```

자주 하는 실수:

- 매크로 정의 끝에 세미콜론을 붙이는 경우 (`#define N 10;`).
- 매크로 이름과 괄호 사이에 공백을 넣는 경우 (`#define SQ (x) ...` 는 함수형 매크로가 아님).
- `16000000` 처럼 큰 상수에 `UL` 을 빠뜨려 AVR의 16비트 연산에서 넘치는 경우.

연습 문제 힌트:

1. `a + 1 * a + 1` 로 펼쳐진 줄을 찾을 수 있습니다.
2. `1U << 2 * 2` 가 되어 `*` 가 먼저 계산되므로 `1U << 4 = 16` 이 됩니다 (기대값 8).
3. `16000000 / (8 * 115200) - 1 = 16` (정수), 실제 보레이트는 `16000000 / (8 * 17) = 117647` 로 오차 약 2.1 %입니다. `#if` 에서는 정수만 쓸 수 있으므로 오차를 1000배 정수로 계산해 비교합니다.

## 2-16 스코프, static, const, volatile

| 키워드 | 붙는 곳 | 의미 |
|---|---|---|
| `static` | 지역 변수 | 함수가 끝나도 값 유지, 초기화는 한 번 |
| `static` | 전역 변수, 함수 | 이 `.c` 파일 안에서만 보임 (모듈 내부용) |
| `const` | 변수 | 바꿀 수 없음, 실수로 덮어쓰는 것을 컴파일러가 막음 |
| `volatile` | 변수 | 컴파일러가 모르게 바뀔 수 있음, 매번 메모리에서 읽고 씀 |

`volatile` 은 ISR(인터럽트 서비스 루틴)과 메인 루프가 공유하는 변수, 하드웨어 레지스터에 필요합니다. 이것이 없으면 최적화된 컴파일러는 루프 안에서 값을 한 번만 읽고 다시 읽지 않아 플래그 변화를 놓칩니다.

`volatile` 은 원자성을 보장하지 않습니다. 8비트 AVR이 `uint32_t` 를 읽는 데는 명령이 여러 개 필요하고, 그 사이에 ISR이 값을 바꾸면 반쯤 바뀐 값을 읽습니다. 그래서 여러 바이트 공유 변수는 인터럽트를 잠깐 막고(`cli()`) 복사합니다. 예제 6번은 이 상황을 바이트 단위 갱신으로 흉내 낸 것입니다.

<!-- include: part02_c/16_volatile_static_const.c -->

실행 결과:

```
== 1. block scope and shadowing ==
inner v = 2
outer v = 1

== 2. static local vs auto local ==
call 1: static=1 auto=1
call 2: static=2 auto=1
call 3: static=3 auto=1
led: 0 1 0 1
led in order: 1 0 1 0

== 3. static global (file scope) ==
g_module_counter = 3

== 4. const ==
LOW_BATT_MV = 6400
sin(30 deg) x1000 = 500
*p1=2 x=5 *p3=2

== 5. volatile flag between ISR and main (simulated) ==
millis= 5 -> control_step #1
millis=10 -> control_step #2
millis=15 -> control_step #3
millis=20 -> control_step #4

== 6. why atomic access matters (simulated torn read) ==
expected 0x00000100, ISR saw 0x00000000 in the middle, final 0x00000100
```

예제 2번의 `led: 0 1 0 1` 은 gcc가 함수 인자를 오른쪽부터 평가했기 때문에 나온 결과입니다. 인자 평가 순서는 C 표준이 정하지 않으므로 컴파일러에 따라 다를 수 있으며, 바로 아래 줄처럼 한 번에 하나씩 호출하는 것이 올바른 방법입니다.

자주 하는 실수:

- ISR과 공유하는 변수에 `volatile` 을 빠뜨려 최적화 빌드(`-Os`)에서만 동작이 멈추는 경우.
- `volatile` 이면 여러 바이트 변수도 안전하다고 생각하는 경우.
- 헤더 파일에 `static` 변수를 정의해 파일마다 별개의 복사본이 생기는 경우.
- 지역 변수 이름을 전역과 같게 지어 가리는 경우 (shadowing).

연습 문제 힌트:

1. `static` 전역도 같은 동작을 하지만 그 파일의 다른 함수가 모두 바꿀 수 있습니다. `static` 지역은 그 함수만 접근하므로 더 안전합니다.
2. (가)와 (다)입니다. (나)는 메인에서만 쓰고, (라)는 바뀌지 않습니다.
3. `uint8_t` 는 명령 하나(`lds`)로 읽히므로 중간에 끼어들 틈이 없습니다. `uint32_t` 는 명령 네 개로 나눠 읽으므로 그 사이에 ISR이 실행될 수 있습니다.

## 2-17 함수 포인터와 명령 테이블

함수 이름은 그 함수의 주소이므로 변수에 담아 나중에 호출할 수 있습니다. `typedef` 로 타입 이름을 만들면 읽기 쉽습니다.

```
typedef int (*CmdHandler)(const char *args);
```

콜백은 "무엇을 할지" 를 함수로 넘기는 방식입니다 (`qsort` 의 비교 함수가 대표적). 명령 테이블은 `{ 명령 문자, 처리 함수 }` 의 배열로, 새 명령을 추가할 때 표에 한 줄만 넣으면 되어 `switch` 가 길어지지 않습니다. Part 8의 블루투스 명령 처리가 이 구조를 씁니다. 응답 `#O,c` 와 `#E,n` 은 SPEC 4장의 오류 번호를 따릅니다.

<!-- include: part02_c/17_function_pointer.c -->

실행 결과:

```
== 1. function pointer basics ==
op = add : op(6, 7) = 13
op = mul : op(6, 7) = 42
ops[0] (add)(3, 4) = 7
ops[1] (mul)(3, 4) = 12

== 2. callbacks ==
  sample[0] = 40
  sample[1] = -10
  sample[2] = 25
  sample[3] = 5
  sample[4] = -30
  sum via callback = 30
  qsort result: -30 -10 5 25 40

== 3. command table dispatch ==
$P,1200,50,80
  -> PID set kp=1200 ki=50 kd=80
  reply #O,P
$T,-35
  -> trim set -35
  reply #O,T
$T,5000
  reply #E,3
$M,0
  -> motor DISABLED
  reply #O,M
$G
  -> #K,1200,50,80,-35,...
  reply #O,G
$S
  -> 'S' has no handler
  reply #E,4
$X,1
  reply #E,4
$P,12
  reply #E,2

== 4. help from the table ==
  P : PID gains x100
  T : trim angle x100
  M : motor enable 0/1
  G : get parameters
  S : save (not implemented in this demo)
```

자주 하는 실수:

- `NULL` 함수 포인터를 호출하는 경우 (AVR에서는 리셋 벡터로 점프해 재시작한 것처럼 보임).
- 시그니처가 다른 함수를 캐스트로 억지로 넣는 경우.
- `int *f(int)` (포인터를 돌려주는 함수)와 `int (*f)(int)` (함수 포인터)를 혼동하는 경우.

연습 문제 힌트:

1. `static int cmd_speed(const char *args)` 를 `cmd_trim` 과 같은 모양으로 만들고 범위를 0..100000으로 검사한 뒤 `{ 'V', cmd_speed, "speed gain x100" }` 을 추가합니다.
2. `typedef void (*StateFn)(Robot *r, ...);` 를 만들고 `STATE_FN[r->state](r, ...)` 로 호출합니다. 범위 밖 값 검사를 잊지 마세요.
3. `typedef int (*Pred)(int16_t);` 를 받아 `for (i...) if (pred(buf[i])) return i; return -1;`

## 2-18 미니 프로젝트: 링버퍼 (다중 파일)

링버퍼(원형 큐)는 고정 크기 배열을 원처럼 돌려 쓰는 FIFO입니다. UART 수신 인터럽트가 바이트를 넣고(`rb_put`), 메인 루프가 꺼내 한 줄 프레임을 조립합니다(`rb_get`). 인터럽트는 언제든 바이트를 넣을 수 있고 메인 루프는 5 ms마다 꺼내므로, 그 사이에 도착한 바이트를 잠시 보관하는 완충 장치입니다.

설계 포인트:

| 항목 | 선택 | 이유 |
|---|---|---|
| 크기 | 2의 거듭제곱 (1..128) | `i % SIZE` 대신 `i & (SIZE - 1)` 사용, AVR에는 나눗셈 명령이 없음 |
| head, tail | 계속 증가하는 `uint8_t` | 개수 = `head - tail`, 순환해도 올바름, 모든 칸 사용 가능 |
| 역할 분리 | ISR은 head만, 메인은 tail만 변경 | 생산자 하나, 소비자 하나면 인터럽트 금지 없이 안전 |
| 가득 참 | 새 바이트를 버리고 `overflow` 증가 | 생산자가 tail을 건드리지 않게 하기 위함 |

파일 구성은 다음과 같습니다.

- `ringbuf.h`: 공개 인터페이스(타입, 함수 원형)와 헤더 가드, 크기 검사 `#error`
- `ringbuf.c`: 구현
- `main.c`: 사용 예와 UART 수신 시뮬레이션

빌드:

```
gcc -Wall -Wextra -std=c99 18_ringbuffer/main.c 18_ringbuffer/ringbuf.c -o ringbuffer
```

헤더 가드는 같은 헤더가 여러 번 포함되어도 내용이 한 번만 들어가게 합니다.

```
#ifndef RINGBUF_H
#define RINGBUF_H
...
#endif
```

<!-- include: part02_c/18_ringbuffer/ringbuf.h -->

<!-- include: part02_c/18_ringbuffer/ringbuf.c -->

<!-- include: part02_c/18_ringbuffer/main.c -->

실행 결과 (`sizeof(RingBuf)` 는 head, tail, overflow 3바이트와 버퍼 8바이트의 합입니다):

```
RB_SIZE=8 RB_MASK=0x07 sizeof(RingBuf)=11

== 1. put and get ==
init                       head=  0 tail=  0 count=0 free=8 overflow=0
put 10,20,30,40,50         head=  5 tail=  0 count=5 free=3 overflow=0
get: 10 20 30
after 3 gets               head=  5 tail=  3 count=2 free=6 overflow=0

== 2. wrap around ==
put 60..110                head= 11 tail=  3 count=8 free=0 overflow=0
slot: [0]= 90 [1]=100 [2]=110 [3]= 40 [4]= 50 [5]= 60 [6]= 70 [7]= 80
get: 40 50 60 70 80 90 100 110
empty again                head= 11 tail= 11 count=0 free=8 overflow=0

== 3. overflow ==
A B C D E F G H I(drop) J(drop) K(drop)
after 11 puts              head= 19 tail= 11 count=8 free=0 overflow=3
get: A B C D E F G H

== 4. head/tail counters roll over 255 -> 0 ==
put 4 bytes from 254       head=  2 tail=254 count=4 free=4 overflow=0
get: 0 1 2 3

== 5. UART simulation: ISR puts, main loop builds frames ==
frame: "$G*47"
frame: "$M,1*50"
frame: "$T,-35*53"
end of stream              head= 25 tail= 25 count=0 free=8 overflow=0

== 6. main loop too slow -> overflow ==
no polling                 head=  8 tail=  0 count=8 free=0 overflow=17
frame: "$G*47"
partial line left: 1 bytes (frames lost)
```

6번은 메인 루프가 제때 꺼내지 않으면 무슨 일이 생기는지 보여 줍니다. 버퍼 8바이트만 남고 나머지 17바이트는 버려져 첫 프레임만 살아남습니다.

`RB_SIZE` 를 2의 거듭제곱이 아닌 값으로 빌드하면 전처리 단계에서 멈춥니다 (실제 출력).

```
$ gcc -DRB_SIZE=6u -Wall -Wextra -std=c99 18_ringbuffer/main.c 18_ringbuffer/ringbuf.c -o build/rb6
In file included from 18_ringbuffer/main.c:16:
18_ringbuffer/ringbuf.h:35:2: error: #error "RB_SIZE must be a power of two between 1 and 128"
   35 | #error "RB_SIZE must be a power of two between 1 and 128"
      |  ^~~~~
In file included from 18_ringbuffer/ringbuf.c:18:
18_ringbuffer/ringbuf.h:35:2: error: #error "RB_SIZE must be a power of two between 1 and 128"
   35 | #error "RB_SIZE must be a power of two between 1 and 128"
      |  ^~~~~
```

자주 하는 실수:

- 헤더에 함수 정의나 변수 정의를 넣어 여러 `.c` 에서 포함할 때 "multiple definition" 링크 오류가 나는 경우. 헤더에는 선언만 둡니다.
- 직접 만든 헤더를 `<ringbuf.h>` 로 포함해 찾지 못하는 경우. `"ringbuf.h"` 로 씁니다.
- `ringbuf.c` 를 빌드 명령에서 빠뜨려 "undefined reference to rb_put" 이 나는 경우.
- 데이터를 쓰기 전에 head를 증가시켜, 그 순간 메인 루프가 아직 쓰이지 않은 칸을 읽는 경우.

연습 문제 힌트 (main.c):

1. `#error "RB_SIZE must be a power of two between 1 and 128"` 이 나옵니다. 2의 거듭제곱이 아니면 마스크로 위치를 구할 수 없고, 256을 나누어 떨어지지 않아 head, tail 순환 시 위치가 어긋납니다.
2. 9바이트마다 꺼내면 8바이트 버퍼가 넘쳐 프레임이 깨집니다. 9600 bps에서 5 ms 동안 약 5바이트가 들어오므로, 메인 루프가 한 번 늦어지는 경우까지 고려해 최대 프레임 48바이트 이상인 64를 씁니다.
3. `rb_get` 과 같지만 `tail` 을 증가시키지 않습니다.

연습 문제 힌트 (ringbuf.h, ringbuf.c):

- 헤더 가드를 지우고 두 번 포함하면 `conflicting types for 'RingBuf'` 같은 오류가 납니다 (같은 타입이 두 번 정의됨).
- 크기 256을 허용하면 가득 찼을 때 `head - tail = 256` 이 `uint8_t` 로 0이 되어 비어 있는 것으로 보입니다. `uint16_t` 인덱스면 32768까지 가능하지만 AVR에서 16비트 읽기는 원자적이지 않아 다시 보호가 필요합니다.
- head 증가를 먼저 하면 메인 루프가 아직 쓰이지 않은 칸을 읽을 수 있습니다. 덮어쓰기 방식은 생산자가 tail을 바꾸므로 소비자와 동시에 tail을 바꾸는 경쟁이 생깁니다.

## Part 3으로 가기 전 체크리스트

아래 항목을 모두 "예" 로 답할 수 있으면 Part 3(AVR 레지스터 프로그래밍)으로 넘어갑니다. 답하기 어려운 항목은 해당 레슨으로 돌아가 예제를 고쳐 보며 확인하세요.

- [ ] `make` 로 18개 실습이 경고 0개로 빌드되고, `make run` 결과가 이 문서와 같다 (2-01)
- [ ] 전처리, 컴파일, 어셈블, 링크 중 "undefined reference" 가 어느 단계 오류인지 안다 (2-01)
- [ ] AVR에서 `int` 가 16비트라는 것과, 그래서 `uint8_t`, `int16_t`, `uint32_t` 를 쓴다는 것을 설명할 수 있다 (2-02)
- [ ] `(int32_t)a * b` 처럼 곱하기 전에 넓혀야 하는 경우를 찾을 수 있다 (2-02, 2-07)
- [ ] `PORTB |= (1 << PB2);`, `PORTB &= ~(1 << PB2);`, `PORTB ^= (1 << PB2);`, `if (PIND & (1 << PD3))` 를 보고 각각 무엇을 하는지 말할 수 있다 (2-04)
- [ ] 마스크로 여러 비트 필드를 다른 비트를 건드리지 않고 바꿀 수 있다 (2-04)
- [ ] `(x & MASK) == VALUE` 에 괄호가 왜 필요한지 안다 (2-03, 2-04)
- [ ] 펌웨어 `main` 이 `for (;;)` 슈퍼 루프로 끝나는 이유를 안다 (2-01, 2-06)
- [ ] 값 전달과 포인터 전달의 차이를 설명하고, 결과 두 개를 돌려주는 함수를 만들 수 있다 (2-07, 2-10)
- [ ] 배열 매개변수에 길이를 함께 넘겨야 하는 이유를 안다 (2-11)
- [ ] 문자열 끝의 `'\0'` 을 고려해 버퍼 크기를 정하고 `snprintf` 를 쓸 수 있다 (2-09)
- [ ] 상위, 하위 두 바이트를 `int16_t` 하나로 올바르게 합칠 수 있다 (2-13)
- [ ] 구조체 포인터와 `->` 로 센서 데이터를 함수에 넘기고 수정할 수 있다 (2-12)
- [ ] `enum` 과 `switch` 로 상태 머신을 만들고 상태 변경을 한 함수로 모을 수 있다 (2-14)
- [ ] 매크로 함수에 괄호를 씌우는 이유와 `#if ... #error` 로 설정값을 검사하는 방법을 안다 (2-15)
- [ ] ISR과 공유하는 변수에 `volatile` 이 필요한 이유, 그리고 `volatile` 만으로는 여러 바이트 변수가 안전하지 않은 이유를 설명할 수 있다 (2-16)
- [ ] `static` 지역 변수와 `static` 전역 변수의 차이를 안다 (2-16)
- [ ] 함수 포인터 표로 명령 문자를 처리 함수에 연결할 수 있다 (2-17)
- [ ] `.h` 와 `.c` 를 나누고 헤더 가드를 쓰며, 여러 파일을 함께 빌드할 수 있다 (2-18)
- [ ] 2의 거듭제곱 링버퍼가 UART 수신에 쓰이는 이유와 넘침(overflow)이 생기는 조건을 설명할 수 있다 (2-18)
