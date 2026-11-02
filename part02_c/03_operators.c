/*
 * 03_operators.c : 연산자와 우선순위
 *
 * [학습 목표]
 *  1. 산술, 대입, 증감, 비교, 논리 연산자의 동작을 정확히 설명할 수 있습니다.
 *  2. 전위(++i)와 후위(i++) 증가의 차이를 구분합니다.
 *  3. 논리 연산자의 단락 평가(short circuit)를 이해합니다.
 *  4. 정수 승격과 부호 있는/없는 비교의 함정을 경험합니다.
 *  5. 헷갈리는 우선순위는 괄호로 명확히 쓰는 습관을 들입니다.
 */

#include <stdio.h>
#include <stdint.h>

/* 단락 평가를 보여 주기 위한 함수: 호출되면 메시지를 출력합니다. */
static int check(const char *name, int result)
{
    printf("  check(%s) called -> %d\n", name, result);
    return result;
}

int main(void)
{
    printf("== 1. arithmetic ==\n");
    int a = 17, b = 5;
    printf("a + b = %d, a - b = %d, a * b = %d\n", a + b, a - b, a * b);
    printf("a / b = %d, a %% b = %d\n", a / b, a % b);
    /* 음수 나눗셈은 C99 부터 0 쪽으로 버림입니다. */
    printf("-17 / 5 = %d, -17 %% 5 = %d\n", -17 / 5, -17 % 5);

    printf("\n== 2. compound assignment ==\n");
    int x = 10;
    x += 5;  printf("x += 5  -> %d\n", x);
    x *= 2;  printf("x *= 2  -> %d\n", x);
    x -= 3;  printf("x -= 3  -> %d\n", x);
    x /= 4;  printf("x /= 4  -> %d\n", x);
    x %= 4;  printf("x %%= 4  -> %d\n", x);

    printf("\n== 3. prefix vs postfix ==\n");
    int i = 5;
    int r1 = i++;   /* r1 에 5 를 넣은 뒤 i 가 6 이 됩니다 */
    printf("r1 = i++ : r1=%d, i=%d\n", r1, i);
    int r2 = ++i;   /* i 를 먼저 7 로 만든 뒤 r2 에 7 을 넣습니다 */
    printf("r2 = ++i : r2=%d, i=%d\n", r2, i);
    /* 주의: i = i++ + ++i; 처럼 한 식에서 같은 변수를 두 번 바꾸면
       정의되지 않은 동작(UB)입니다. 절대 쓰지 않습니다. */

    printf("\n== 4. comparison and logical ==\n");
    int t = 25;
    printf("t > 20 && t < 30 : %d\n", t > 20 && t < 30);
    printf("t < 0 || t > 100 : %d\n", t < 0 || t > 100);
    printf("!(t == 25)       : %d\n", !(t == 25));
    /* C 에서 참은 0 이 아닌 모든 값, 비교 결과는 항상 0 또는 1 입니다. */

    printf("\n== 5. short circuit ==\n");
    printf("check(A,0) && check(B,1):\n");
    if (check("A", 0) && check("B", 1)) { /* 없음 */ }
    printf("check(A,1) || check(B,0):\n");
    if (check("A", 1) || check("B", 0)) { /* 없음 */ }
    /* 왼쪽만으로 결과가 정해지면 오른쪽은 실행되지 않습니다.
       그래서 if (p != NULL && p->value > 0) 같은 안전 검사가 가능합니다. */

    printf("\n== 6. conditional (ternary) ==\n");
    int pwm = -300;
    int clamped = (pwm > 255) ? 255 : (pwm < -255) ? -255 : pwm;
    printf("clamp(-300) = %d\n", clamped);

    printf("\n== 7. signed / unsigned comparison trap ==\n");
    unsigned int u = 1;
    int s = -1;
    /* s 가 unsigned 로 변환되어 아주 큰 수(4294967295)가 되므로 결과가 뒤집힙니다.
       -Wextra 를 켜면 이런 비교에 경고가 나오므로 아래처럼 명시적으로 맞춥니다. */
    printf("(int)u > s            : %d (expected)\n", (int)u > s);
    printf("u > (unsigned int)s   : %d (surprise!)\n", u > (unsigned int)s);

    printf("\n== 8. integer promotion with uint8_t ==\n");
    uint8_t p = 200, q = 100;
    /* uint8_t 끼리 더해도 계산은 int 로 승격되어 이뤄집니다.
       결과를 uint8_t 에 담을 때 비로소 잘립니다. */
    int     sum_int = p + q;
    uint8_t sum_u8  = (uint8_t)(p + q);
    printf("p + q as int   = %d\n", sum_int);
    printf("p + q as uint8 = %u\n", (unsigned)sum_u8);

    printf("\n== 9. precedence ==\n");
    int m = 2 + 3 * 4;       /* * 가 먼저: 14 */
    int n = (2 + 3) * 4;     /* 괄호가 먼저: 20 */
    printf("2 + 3 * 4 = %d, (2 + 3) * 4 = %d\n", m, n);
    /* 비트 연산자 & 는 == 보다 우선순위가 낮습니다. 다음 레슨에서 자세히 봅니다. */
    int flags = 0x06;
    printf("(flags & 0x04) == 0x04 : %d\n", (flags & 0x04) == 0x04);

    return 0;
}

/*
 * [연습 문제]
 *  1. int k = 3; int z = k++ * 2; 실행 뒤 k 와 z 의 값을 예측하고 확인해 보세요.
 *  2. 배터리 전압 mv 가 6400 이상 8500 이하일 때만 1 을 돌려주는
 *     식을 && 를 사용해 한 줄로 작성해 보세요.
 *  3. 7번 예제에서 (int)u 캐스트를 지우고 u > s 로 바꾸어 빌드하면
 *     어떤 경고가 나오는지 확인하고, 그 의미를 설명해 보세요.
 */
