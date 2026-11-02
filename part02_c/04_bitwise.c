/*
 * 04_bitwise.c : 비트 연산 (임베디드에서 가장 많이 쓰는 C 문법)
 *
 * [학습 목표]
 *  1. &, |, ^, ~, <<, >> 연산을 2진수로 계산할 수 있습니다.
 *  2. 비트 set / clear / toggle / test 를 매크로로 만들어 쓸 수 있습니다.
 *  3. 마스크로 여러 비트 필드를 읽고 쓰는 방법(읽기, 수정, 쓰기)을 익힙니다.
 *  4. PC 에서 가짜 레지스터 변수로 AVR 의 DDRB, PORTB 조작을 흉내 냅니다.
 */

#include <stdio.h>
#include <stdint.h>

/* ---------- 비트 조작 매크로 ----------
 * 인자에 괄호를 두르는 이유는 15_preprocessor.c 에서 자세히 다룹니다.
 * 1U 를 쓰는 이유: 1 은 int 이므로 1 << 31 은 부호 비트를 건드려 UB 가 됩니다. */
#define BIT(n)              (1U << (n))
#define SET_BIT(reg, n)     ((reg) |=  BIT(n))
#define CLEAR_BIT(reg, n)   ((reg) &= (uint8_t)~BIT(n))
#define TOGGLE_BIT(reg, n)  ((reg) ^=  BIT(n))
#define TEST_BIT(reg, n)    (((reg) >> (n)) & 1U)

/* 8비트 값을 2진수 문자열로 출력하는 도우미 함수 */
static void print_bin8(const char *label, uint8_t v)
{
    printf("%-22s 0b", label);
    for (int i = 7; i >= 0; i--) {
        putchar(((v >> i) & 1U) ? '1' : '0');
        if (i == 4) putchar('_');
    }
    printf("  (0x%02X, %3u)\n", (unsigned)v, (unsigned)v);
}

/* ---------- 가짜 레지스터 ----------
 * 실제 AVR 에서는 DDRB, PORTB 가 정해진 메모리 주소의 레지스터입니다.
 * 여기서는 같은 이름의 전역 변수로 흉내 냅니다. 비트 번호 이름도 AVR 헤더와
 * 같은 방식(PB0 = 0, PB1 = 1 ...)으로 정의합니다. */
static uint8_t DDRB  = 0x00;  /* 방향 레지스터: 1 = 출력, 0 = 입력 */
static uint8_t PORTB = 0x00;  /* 출력 레지스터: 1 = High, 0 = Low  */
static uint8_t PIND  = 0xFF;  /* 입력 레지스터 (풀업이라 평소 1)    */

#define PB0 0   /* D8  : ML_IN2   */
#define PB1 1   /* D9  : MR_IN4   */
#define PB2 2   /* D10 : LED_STAT */
#define PD3 3   /* D3  : BTN      */

int main(void)
{
    printf("== 1. basic bit operators ==\n");
    uint8_t a = 0xCA;   /* 1100_1010 */
    uint8_t b = 0x0F;   /* 0000_1111 */
    print_bin8("a", a);
    print_bin8("b", b);
    print_bin8("a & b  (AND)", (uint8_t)(a & b));
    print_bin8("a | b  (OR)",  (uint8_t)(a | b));
    print_bin8("a ^ b  (XOR)", (uint8_t)(a ^ b));
    print_bin8("~a     (NOT)", (uint8_t)~a);
    print_bin8("a << 2", (uint8_t)(a << 2));
    print_bin8("a >> 2", (uint8_t)(a >> 2));

    printf("\n== 2. set / clear / toggle / test ==\n");
    uint8_t r = 0x00;
    SET_BIT(r, 3);      print_bin8("SET_BIT(r,3)", r);
    SET_BIT(r, 0);      print_bin8("SET_BIT(r,0)", r);
    TOGGLE_BIT(r, 7);   print_bin8("TOGGLE_BIT(r,7)", r);
    CLEAR_BIT(r, 3);    print_bin8("CLEAR_BIT(r,3)", r);
    printf("TEST_BIT(r,7) = %u, TEST_BIT(r,3) = %u\n",
           TEST_BIT(r, 7), TEST_BIT(r, 3));

    printf("\n== 3. mask: read-modify-write a field ==\n");
    /* 상위 3비트(7..5)를 "모드" 필드로, 하위 5비트는 다른 설정으로 쓰는 레지스터라고 가정합니다.
       다른 비트를 건드리지 않고 모드만 5(101)로 바꾸려면
         1) 마스크로 모드 비트를 지우고  2) 새 값을 위치에 맞게 시프트해 OR 합니다. */
    uint8_t ctrl = 0x3B;                    /* 001_11011 */
    const uint8_t MODE_MASK  = 0xE0;        /* 111_00000 */
    const uint8_t MODE_SHIFT = 5;
    print_bin8("ctrl before", ctrl);
    ctrl = (uint8_t)((ctrl & ~MODE_MASK) | ((5U << MODE_SHIFT) & MODE_MASK));
    print_bin8("ctrl after (mode=5)", ctrl);
    printf("read mode = %u\n", (unsigned)((ctrl & MODE_MASK) >> MODE_SHIFT));

    printf("\n== 4. register-like usage (fake DDRB / PORTB) ==\n");
    /* LED(PB2)와 모터 방향 핀(PB0, PB1)을 출력으로 설정합니다.
       AVR 코드에서도 똑같이 DDRB |= (1 << PB2); 처럼 씁니다. */
    DDRB |= (uint8_t)(BIT(PB0) | BIT(PB1) | BIT(PB2));
    print_bin8("DDRB (PB0..PB2 out)", DDRB);

    PORTB |= (uint8_t)BIT(PB2);            /* LED 켜기 */
    print_bin8("PORTB LED on", PORTB);

    PORTB |= (uint8_t)BIT(PB0);            /* ML_IN2 = 1 */
    PORTB &= (uint8_t)~BIT(PB1);           /* MR_IN4 = 0 */
    print_bin8("PORTB IN2=1, IN4=0", PORTB);

    for (int k = 0; k < 3; k++) {          /* LED 깜빡이기 흉내 */
        PORTB ^= (uint8_t)BIT(PB2);
        printf("blink %d: LED=%u\n", k, (unsigned)TEST_BIT(PORTB, PB2));
    }

    /* 버튼은 풀업이므로 누르지 않으면 1, 누르면 0 입니다 (Active Low). */
    printf("button pressed? %s\n", (PIND & BIT(PD3)) ? "no" : "yes");
    PIND &= (uint8_t)~BIT(PD3);            /* 버튼을 눌렀다고 가정 */
    printf("button pressed? %s\n", (PIND & BIT(PD3)) ? "no" : "yes");

    printf("\n== 5. combine / split 16-bit with shifts ==\n");
    uint8_t hi = 0xFC, lo = 0x18;
    int16_t value = (int16_t)(((uint16_t)hi << 8) | lo);
    printf("hi=0x%02X lo=0x%02X -> 0x%04X = %d\n",
           (unsigned)hi, (unsigned)lo, (unsigned)(uint16_t)value, value);
    printf("split back: hi=0x%02X lo=0x%02X\n",
           (unsigned)(((uint16_t)value >> 8) & 0xFF), (unsigned)((uint16_t)value & 0xFF));

    printf("\n== 6. precedence trap ==\n");
    uint8_t status = 0x04;
    /* == 가 & 보다 먼저 계산되므로 status & (0x04 == 0x04) 가 되어 버립니다. */
    printf("status & 0x04 == 0x04   -> %d (wrong)\n", status & (0x04 == 0x04));
    printf("(status & 0x04) == 0x04 -> %d (right)\n", (status & 0x04) == 0x04);

    return 0;
}

/*
 * [연습 문제]
 *  1. 8비트 값에서 1인 비트의 개수를 세는 함수 count_ones(uint8_t v) 를 작성해
 *     0xCA 에 대해 확인해 보세요 (정답 4).
 *  2. PORTB 의 PB0 과 PB1 을 동시에 반전하는 한 줄 코드를 작성해 보세요.
 *  3. 2의 거듭제곱인지 검사하는 식 (x != 0) && ((x & (x - 1)) == 0) 이
 *     왜 맞는지 2진수로 설명해 보세요. 18_ringbuffer 에서 다시 씁니다.
 */
