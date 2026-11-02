/*
 * 17_function_pointer.c : 함수 포인터, 콜백, 명령 테이블 디스패치
 *
 * [학습 목표]
 *  1. 함수 포인터를 선언하고 typedef 로 읽기 쉽게 만듭니다.
 *  2. 콜백(callback): 라이브러리 함수에 "할 일" 을 함수로 넘기는 방식을 익힙니다.
 *  3. 명령 문자와 처리 함수를 묶은 표(배열)로 switch 없이 명령을 분기합니다
 *     (프로토콜 v1 의 $D, $P, $T, $M, $S, $G 처리 구조).
 *  4. 함수 포인터가 NULL 일 때의 안전 검사를 습관화합니다.
 */

#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>   /* qsort */

/* ---------- 1. 기본 ---------- */
static int add(int a, int b) { return a + b; }
static int mul(int a, int b) { return a * b; }

/* 함수 포인터 타입: int 두 개를 받아 int 를 돌려주는 함수를 가리킵니다. */
typedef int (*BinOp)(int, int);

/* ---------- 2. 콜백 ----------
 * 배열의 각 요소에 대해 호출자가 넘긴 함수를 실행합니다.
 * "무엇을 할지" 는 호출하는 쪽이 정합니다. */
typedef void (*SampleCallback)(int index, int16_t value);

static void for_each_sample(const int16_t *buf, int n, SampleCallback cb)
{
    if (cb == NULL) return;
    for (int i = 0; i < n; i++) {
        cb(i, buf[i]);
    }
}

static void print_sample(int index, int16_t value)
{
    printf("  sample[%d] = %d\n", index, value);
}

static int32_t g_sum = 0;
static void sum_sample(int index, int16_t value)
{
    (void)index;      /* 쓰지 않는 매개변수 경고(-Wextra)를 없애는 관용 표현 */
    g_sum += value;
}

/* 표준 라이브러리 qsort 도 비교 함수를 콜백으로 받습니다. */
static int cmp_i16(const void *a, const void *b)
{
    int16_t x = *(const int16_t *)a;
    int16_t y = *(const int16_t *)b;
    return (x > y) - (x < y);
}

/* ---------- 3. 명령 테이블 ---------- */
typedef struct {
    int32_t kp, ki, kd, trim;
    int     motor_enable;
} Params;

static Params g_params = { 2500, 10, 4000, 0, 1 };

/* 모든 명령 처리 함수는 같은 모양(시그니처)을 가집니다.
   args 는 명령 문자 다음의 문자열입니다 (예 "1200,50,80"). 반환값은 오류 번호(0 = 성공). */
typedef int (*CmdHandler)(const char *args);

static int cmd_pid(const char *args)
{
    long kp, ki, kd;
    if (sscanf(args, "%ld,%ld,%ld", &kp, &ki, &kd) != 3) return 2;   /* 형식 오류 */
    if (kp < 0 || kp > 100000 || ki < 0 || ki > 100000 || kd < 0 || kd > 100000) return 3;
    g_params.kp = kp; g_params.ki = ki; g_params.kd = kd;
    printf("  -> PID set kp=%ld ki=%ld kd=%ld\n", kp, ki, kd);
    return 0;
}

static int cmd_trim(const char *args)
{
    long t;
    if (sscanf(args, "%ld", &t) != 1) return 2;
    if (t < -1000 || t > 1000) return 3;                                 /* 범위 오류 */
    g_params.trim = t;
    printf("  -> trim set %ld\n", t);
    return 0;
}

static int cmd_motor(const char *args)
{
    int e;
    if (sscanf(args, "%d", &e) != 1) return 2;
    if (e != 0 && e != 1) return 3;
    g_params.motor_enable = e;
    printf("  -> motor %s\n", e ? "ENABLED" : "DISABLED");
    return 0;
}

static int cmd_get(const char *args)
{
    (void)args;
    printf("  -> #K,%ld,%ld,%ld,%ld,...\n", (long)g_params.kp, (long)g_params.ki,
           (long)g_params.kd, (long)g_params.trim);
    return 0;
}

typedef struct {
    char       code;       /* 명령 문자 */
    CmdHandler handler;    /* 처리 함수 */
    const char *help;
} CmdEntry;

/* 새 명령을 추가할 때 이 표에 한 줄만 추가하면 됩니다. const 로 두어 바뀌지 않게 합니다. */
static const CmdEntry CMD_TABLE[] = {
    { 'P', cmd_pid,   "PID gains x100" },
    { 'T', cmd_trim,  "trim angle x100" },
    { 'M', cmd_motor, "motor enable 0/1" },
    { 'G', cmd_get,   "get parameters" },
    { 'S', NULL,      "save (not implemented in this demo)" },
};
#define CMD_COUNT (sizeof(CMD_TABLE) / sizeof(CMD_TABLE[0]))

/* "P,1200,50,80" 같은 본문을 받아 표에서 찾아 실행합니다. */
static int dispatch(const char *body)
{
    char code = body[0];
    const char *args = (body[1] == ',') ? body + 2 : body + 1;
    for (size_t i = 0; i < CMD_COUNT; i++) {
        if (CMD_TABLE[i].code == code) {
            if (CMD_TABLE[i].handler == NULL) {
                printf("  -> '%c' has no handler\n", code);
                return 4;
            }
            return CMD_TABLE[i].handler(args);
        }
    }
    return 4;   /* 알 수 없는 명령 */
}

int main(void)
{
    printf("== 1. function pointer basics ==\n");
    BinOp op = add;                    /* & 없이 함수 이름만 써도 주소가 됩니다 */
    printf("op = add : op(6, 7) = %d\n", op(6, 7));
    op = mul;
    printf("op = mul : op(6, 7) = %d\n", op(6, 7));

    BinOp ops[2] = { add, mul };       /* 함수 포인터 배열 */
    const char *op_names[2] = { "add", "mul" };
    for (int i = 0; i < 2; i++) {
        printf("ops[%d] (%s)(3, 4) = %d\n", i, op_names[i], ops[i](3, 4));
    }

    printf("\n== 2. callbacks ==\n");
    int16_t samples[5] = { 40, -10, 25, 5, -30 };
    for_each_sample(samples, 5, print_sample);
    g_sum = 0;
    for_each_sample(samples, 5, sum_sample);
    printf("  sum via callback = %ld\n", (long)g_sum);
    for_each_sample(samples, 5, NULL);  /* NULL 이면 아무 일도 하지 않습니다 */

    qsort(samples, 5, sizeof(samples[0]), cmp_i16);
    printf("  qsort result:");
    for (int i = 0; i < 5; i++) printf(" %d", samples[i]);
    printf("\n");

    printf("\n== 3. command table dispatch ==\n");
    const char *bodies[] = { "P,1200,50,80", "T,-35", "T,5000", "M,0", "G", "S", "X,1", "P,12" };
    for (size_t i = 0; i < sizeof(bodies) / sizeof(bodies[0]); i++) {
        printf("$%s\n", bodies[i]);
        int err = dispatch(bodies[i]);
        if (err == 0) {
            printf("  reply #O,%c\n", bodies[i][0]);
        } else {
            printf("  reply #E,%d\n", err);
        }
    }

    printf("\n== 4. help from the table ==\n");
    for (size_t i = 0; i < CMD_COUNT; i++) {
        printf("  %c : %s\n", CMD_TABLE[i].code, CMD_TABLE[i].help);
    }
    return 0;
}

/*
 * [연습 문제]
 *  1. 'V' (속도 보상 게인, 0..100000) 명령 처리 함수를 만들고 CMD_TABLE 에
 *     한 줄 추가해 "$V,250" 이 동작하게 해 보세요.
 *  2. 14장의 상태 머신을 "상태마다 처리 함수" 를 가진 함수 포인터 배열
 *     (StateFn STATE_FN[ST_COUNT]) 로 바꿔 보세요.
 *  3. for_each_sample 처럼 콜백을 받는 함수 find_first(buf, n, pred) 를 만들어,
 *     pred 가 참을 돌려주는 첫 인덱스를 찾게 해 보세요 (예: 음수 찾기).
 */
