/*
 * 10_pointers.c : 포인터 기초
 *
 * [학습 목표]
 *  1. 변수의 주소(&)와 포인터 변수, 역참조(*)를 설명할 수 있습니다.
 *  2. 포인터로 함수 밖의 변수를 바꾸는 방법(주소 전달)을 익힙니다.
 *  3. 여러 개의 결과를 포인터 매개변수로 돌려받는 패턴을 씁니다.
 *  4. NULL 포인터 검사와 const 포인터의 의미를 압니다.
 *  5. AVR 레지스터 접근이 결국 "정해진 주소를 가리키는 포인터" 라는 것을 이해합니다.
 */

#include <stdio.h>
#include <stdint.h>

/* 값 두 개를 바꿉니다. 주소를 받아야 원본을 바꿀 수 있습니다. */
static void swap16(int16_t *a, int16_t *b)
{
    int16_t tmp = *a;
    *a = *b;
    *b = tmp;
}

/* 결과를 두 개 돌려주고 싶을 때: 반환값은 성공 여부, 결과는 포인터로 */
static int divide(int32_t num, int32_t den, int32_t *quot, int32_t *rem)
{
    if (den == 0 || quot == NULL || rem == NULL) {
        return 0;          /* 실패 */
    }
    *quot = num / den;
    *rem  = num % den;
    return 1;              /* 성공 */
}

/* const int16_t *p : p 가 가리키는 값을 이 함수 안에서 바꾸지 않겠다는 약속입니다. */
static void print_value(const char *label, const int16_t *p)
{
    if (p == NULL) {
        printf("%s: (null)\n", label);
        return;
    }
    printf("%s: %d\n", label, *p);
    /* *p = 0; 이라고 쓰면 컴파일 오류가 납니다. */
}

int main(void)
{
    printf("== 1. address and dereference ==\n");
    int16_t speed = 120;
    int16_t *ptr = &speed;        /* ptr 은 speed 의 주소를 담습니다 */

    printf("speed       = %d\n", speed);
    printf("*ptr        = %d (value at the address)\n", *ptr);
    printf("ptr == &speed ? %s\n", (ptr == &speed) ? "yes" : "no");
    printf("sizeof(ptr) = %u bytes on this PC (2 bytes on AVR)\n", (unsigned)sizeof(ptr));

    *ptr = 200;                   /* 포인터를 통해 원본을 바꿉니다 */
    printf("after *ptr = 200 -> speed = %d\n", speed);

    printf("\n== 2. swap through pointers ==\n");
    int16_t left = 100, right = -50;
    printf("before: left=%d right=%d\n", left, right);
    swap16(&left, &right);
    printf("after : left=%d right=%d\n", left, right);

    printf("\n== 3. multiple outputs ==\n");
    int32_t q, r;
    if (divide(7820, 1000, &q, &r)) {
        printf("7820 mV = %ld.%03ld V\n", (long)q, (long)r);
    }
    if (!divide(10, 0, &q, &r)) {
        printf("divide by zero rejected\n");
    }

    printf("\n== 4. NULL and const ==\n");
    int16_t trim = -35;
    print_value("trim", &trim);
    print_value("none", NULL);

    printf("\n== 5. pointer to pointer (just a glance) ==\n");
    int16_t *selected = &left;
    int16_t **pp = &selected;     /* 포인터의 주소 */
    **pp = 77;                    /* selected 가 가리키는 left 가 바뀝니다 */
    printf("left=%d\n", left);

    printf("\n== 6. registers are memory addresses ==\n");
    /* AVR 의 <avr/io.h> 는 PORTB 를 대략 이렇게 정의합니다.
     *
     *   #define PORTB (*(volatile uint8_t *)0x25)
     *
     * 0x25 라는 숫자를 "uint8_t 를 가리키는 포인터" 로 바꾸고, 바로 역참조합니다.
     * 그래서 PORTB = 0x04; 는 "주소 0x25 에 0x04 를 쓴다" 는 뜻이 됩니다.
     * volatile 의 의미는 16_volatile_static_const.c 에서 다룹니다.
     *
     * PC 에서 0x25 번지에 쓰면 프로그램이 죽으므로, 배열 하나를 가짜 메모리로 두고
     * 같은 방식의 매크로를 만들어 봅니다. */
    static uint8_t fake_io[0x40];
    #define FAKE_PORTB (*(volatile uint8_t *)&fake_io[0x25])
    FAKE_PORTB = 0x04;
    FAKE_PORTB |= 0x01;
    printf("fake_io[0x25] = 0x%02X\n", (unsigned)fake_io[0x25]);

    printf("\n== 7. dangling pointer (concept) ==\n");
    /* 함수의 지역 변수 주소를 반환하면, 함수가 끝난 뒤 그 메모리는 다른 용도로
       재사용됩니다. 그런 포인터를 "댕글링 포인터" 라 하며 절대 역참조하면 안 됩니다.
       gcc 는 return &local; 에 경고를 냅니다. */
    printf("never return the address of a local variable\n");

    return 0;
}

/*
 * [연습 문제]
 *  1. int16_t 값 하나를 받아 [-255, 255] 로 제한하되, 원본 변수를 직접 바꾸는
 *     void clamp_inplace(int16_t *v) 를 작성해 보세요.
 *  2. 배열의 최솟값과 최댓값을 한 번에 돌려주는
 *     void min_max(const int16_t *a, int n, int16_t *mn, int16_t *mx) 를 작성해 보세요.
 *  3. 6번 예제처럼 FAKE_DDRB 를 fake_io[0x24] 에 정의하고, PB2 를 출력으로
 *     설정한 결과를 출력해 보세요.
 */
