/*
 * 07_functions.c : 함수, 값 전달, 재귀, 스택
 *
 * [학습 목표]
 *  1. 함수 선언(원형)과 정의를 구분하고, 원형이 왜 필요한지 설명합니다.
 *  2. C 는 항상 "값 전달" 이라는 것을 확인합니다 (복사본이 넘어갑니다).
 *  3. 지역 변수가 스택에 놓이고, 함수가 끝나면 사라진다는 것을 이해합니다.
 *  4. 재귀가 스택을 소모하므로 RAM 2 KB 인 ATmega328P 에서 주의해야 함을 압니다.
 *  5. 작은 기능 단위 함수(clamp, map)를 만들어 재사용합니다.
 */

#include <stdio.h>
#include <stdint.h>

/* ---------- 함수 원형(선언) ----------
 * 컴파일러는 위에서 아래로 읽습니다. main 보다 아래에 정의한 함수를
 * main 에서 쓰려면 먼저 "이런 함수가 있다" 고 알려 줘야 합니다. */
static int16_t clamp16(int16_t v, int16_t lo, int16_t hi);
static long    map_range(long x, long in_min, long in_max, long out_min, long out_max);
static void    try_change(int value);
static unsigned long factorial(unsigned n);
static unsigned depth_probe(unsigned depth, unsigned max_depth, const char *base);

int main(void)
{
    printf("== 1. small reusable functions ==\n");
    printf("clamp16(300, -255, 255)  = %d\n", clamp16(300, -255, 255));
    printf("clamp16(-80, -255, 255)  = %d\n", clamp16(-80, -255, 255));
    /* 조이스틱 -100..100 을 PWM -255..255 로 바꾸기 */
    printf("map_range(50, -100, 100, -255, 255) = %ld\n",
           map_range(50, -100, 100, -255, 255));
    /* ADC 0..1023 을 0..5000 mV 로 바꾸기 */
    printf("map_range(512, 0, 1023, 0, 5000)    = %ld\n",
           map_range(512, 0, 1023, 0, 5000));

    printf("\n== 2. pass by value ==\n");
    int speed = 100;
    printf("before call: speed = %d\n", speed);
    try_change(speed);
    printf("after call : speed = %d (unchanged)\n", speed);
    /* 원본을 바꾸려면 주소(포인터)를 넘겨야 합니다. 10_pointers.c 에서 다룹니다. */

    printf("\n== 3. recursion ==\n");
    for (unsigned n = 0; n <= 10; n += 5) {
        printf("%u! = %lu\n", n, factorial(n));
    }

    printf("\n== 4. stack grows with each call ==\n");
    /* 지역 변수의 주소를 비교해 호출이 깊어질수록 스택을 얼마나 쓰는지 봅니다.
       정확한 값은 컴파일러와 최적화 옵션마다 다릅니다. */
    char base_marker;
    depth_probe(1, 4, &base_marker);

    return 0;
}

/* 값을 [lo, hi] 범위로 제한합니다. 모터 PWM 제한에 그대로 씁니다. */
static int16_t clamp16(int16_t v, int16_t lo, int16_t hi)
{
    if (v < lo) return lo;
    if (v > hi) return hi;
    return v;
}

/* 한 범위의 값을 다른 범위로 비례 변환합니다 (Arduino 의 map 과 같은 동작).
   곱셈이 먼저 오도록 써야 정수 나눗셈 오차가 줄어듭니다. */
static long map_range(long x, long in_min, long in_max, long out_min, long out_max)
{
    return (x - in_min) * (out_max - out_min) / (in_max - in_min) + out_min;
}

/* 매개변수 value 는 호출한 쪽 변수의 "복사본" 입니다. */
static void try_change(int value)
{
    value = 0;
    printf("inside func: value = %d\n", value);
}

/* 재귀: 자기 자신을 다시 호출합니다. 반드시 끝나는 조건(n <= 1)이 있어야 합니다.
   호출마다 스택 프레임(복귀 주소, 매개변수, 지역 변수)이 쌓입니다. */
static unsigned long factorial(unsigned n)
{
    if (n <= 1) {
        return 1;
    }
    return n * factorial(n - 1);
}

/* 호출 깊이마다 지역 배열 하나를 만들고, 그 주소가 기준점에서 얼마나
   떨어졌는지 출력합니다. 서로 다른 객체의 주소 차이를 계산하는 것은
   표준상 엄밀하지 않으므로, 정수로 바꾸어 "관찰용" 으로만 씁니다. */
static unsigned depth_probe(unsigned depth, unsigned max_depth, const char *base)
{
    volatile char local[32];       /* 스택을 쓰는 지역 배열 */
    local[0] = (char)depth;
    long diff = (long)((uintptr_t)base - (uintptr_t)&local[0]);
    printf("depth %u: local is about %ld bytes below main's variable\n", depth, diff);
    if (depth < max_depth) {
        return depth_probe(depth + 1, max_depth, base) + (unsigned)local[0];
    }
    return (unsigned)local[0];
}

/*
 * [스택과 ATmega328P]
 *  ATmega328P 의 SRAM 은 2048 바이트뿐입니다. 전역 변수, 힙, 스택이 이 공간을
 *  나눠 씁니다. 재귀가 깊어지거나 큰 지역 배열을 만들면 스택이 전역 변수 영역을
 *  덮어써서 원인을 알기 어려운 오동작이 생깁니다 (스택 오버플로).
 *  그래서 펌웨어에서는 재귀를 쓰지 않고, 큰 버퍼는 static 또는 전역으로 둡니다.
 *  인터럽트(ISR)도 호출될 때 스택을 사용한다는 점을 기억해 두세요.
 *
 * [연습 문제]
 *  1. factorial 을 재귀 없이 for 문으로 다시 작성해 보세요.
 *     unsigned long 이 넘치는 n 은 몇부터인지 이 PC 에서 확인해 보세요.
 *  2. map_range 의 계산 순서를 (x - in_min) / (in_max - in_min) * (out_max - out_min)
 *     으로 바꾸면 결과가 어떻게 달라지는지 확인하고 이유를 설명해 보세요.
 *  3. 두 int16_t 값의 평균을 넘침 없이 구하는 함수 avg16(a, b) 를 작성해 보세요.
 */
