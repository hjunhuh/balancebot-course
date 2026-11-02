/* test_util.h : 아주 작은 단위 테스트 매크로 */
#ifndef TEST_UTIL_H
#define TEST_UTIL_H
#include <stdio.h>
#include <math.h>

static int g_fail = 0;
static int g_pass = 0;

#define CHECK(cond) do { \
    if (cond) { g_pass++; } \
    else { g_fail++; printf("  FAIL %s:%d  %s\n", __FILE__, __LINE__, #cond); } \
} while (0)

#define CHECK_NEAR(a, b, tol) CHECK(fabs((double)(a) - (double)(b)) <= (double)(tol))

#define TEST_REPORT(name) do { \
    printf("[%s] pass %d, fail %d\n", name, g_pass, g_fail); \
    return g_fail ? 1 : 0; \
} while (0)

#endif
