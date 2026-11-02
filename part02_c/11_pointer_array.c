/*
 * 11_pointer_array.c : 포인터 산술, 배열 매개변수, 버퍼 처리
 *
 * [학습 목표]
 *  1. 배열 이름이 대부분의 식에서 첫 요소의 주소로 바뀐다는 것을 이해합니다.
 *  2. p + 1 이 "다음 요소" 를 가리키며, 증가량이 sizeof(요소) 바이트임을 확인합니다.
 *  3. 함수에 배열을 넘기면 포인터가 넘어가므로 길이를 따로 넘겨야 함을 압니다.
 *  4. 포인터로 바이트 버퍼를 훑으며 한 줄(프레임)을 잘라 내고 체크섬을 계산합니다.
 */

#include <stdio.h>
#include <stdint.h>
#include <string.h>

/* 배열 매개변수: const int16_t a[] 와 const int16_t *a 는 완전히 같습니다.
   어느 쪽으로 쓰든 함수가 받는 것은 포인터이므로, 함수 안에서 sizeof(a) 는
   포인터 크기입니다. 그래서 길이 n 을 반드시 함께 받습니다.
   (a[] 형태로 선언하고 sizeof(a) 를 쓰면 gcc 가 -Wsizeof-array-argument 경고로
   알려 줍니다. 여기서는 경고 없이 관찰하려고 포인터 형태로 선언했습니다.) */
static int32_t sum16(const int16_t *a, size_t n)
{
    int32_t s = 0;
    printf("  inside sum16: sizeof(a) = %u (pointer!)\n", (unsigned)sizeof(a));
    for (size_t i = 0; i < n; i++) {
        s += a[i];          /* a[i] 는 *(a + i) 와 같습니다 */
    }
    return s;
}

/* 포인터를 움직이며 처리하는 스타일 */
static void scale_inplace(int16_t *p, size_t n, int16_t factor)
{
    int16_t *end = p + n;   /* 마지막 요소 다음 위치 (가리키기만 하고 읽지 않습니다) */
    while (p < end) {
        *p = (int16_t)(*p * factor);
        p++;
    }
}

/* 프로토콜 v1 체크섬: 시작 문자 다음부터 '*' 직전까지의 XOR */
static uint8_t xor_checksum(const char *begin, const char *end)
{
    uint8_t cs = 0;
    for (const char *p = begin; p < end; p++) {
        cs ^= (uint8_t)*p;
    }
    return cs;
}

/* 수신 버퍼에서 '\n' 으로 끝나는 한 줄을 찾아 line 에 복사합니다.
   '\r' 은 무시합니다. 찾으면 소비한 바이트 수를, 없으면 0 을 돌려줍니다. */
static size_t take_line(const char *buf, size_t len, char *line, size_t line_size)
{
    const char *p = buf;
    const char *end = buf + len;
    size_t out = 0;
    while (p < end) {
        char c = *p++;
        if (c == '\n') {
            line[out] = '\0';
            return (size_t)(p - buf);   /* 포인터끼리 빼면 요소 개수 차이 */
        }
        if (c == '\r') continue;
        if (out + 1 < line_size) {
            line[out++] = c;
        }
    }
    return 0;
}

int main(void)
{
    printf("== 1. array name decays to pointer ==\n");
    int16_t raw[5] = { 10, 20, 30, 40, 50 };
    int16_t *p = raw;                  /* &raw[0] 과 같습니다 */
    printf("raw[2] = %d, *(p + 2) = %d, p[2] = %d\n", raw[2], *(p + 2), p[2]);

    printf("\n== 2. pointer arithmetic step = sizeof(element) ==\n");
    uint8_t  b8[4];
    int16_t  b16[4];
    uint32_t b32[4];
    printf("uint8_t : (p+1) - p = %ld bytes\n",
           (long)((const char *)(b8 + 1) - (const char *)b8));
    printf("int16_t : (p+1) - p = %ld bytes\n",
           (long)((const char *)(b16 + 1) - (const char *)b16));
    printf("uint32_t: (p+1) - p = %ld bytes\n",
           (long)((const char *)(b32 + 1) - (const char *)b32));
    printf("element distance b16+3 - b16 = %ld elements\n", (long)((b16 + 3) - b16));

    printf("\n== 3. array as function parameter ==\n");
    printf("sizeof(raw) in main = %u\n", (unsigned)sizeof(raw));
    printf("sum = %ld\n", (long)sum16(raw, sizeof(raw) / sizeof(raw[0])));

    scale_inplace(raw, 5, 3);
    printf("scaled: ");
    for (int i = 0; i < 5; i++) printf("%d ", raw[i]);
    printf("\n");

    printf("\n== 4. sub-array by pointer offset ==\n");
    /* 배열의 중간부터 처리하고 싶으면 시작 주소만 옮겨서 넘기면 됩니다. */
    printf("sum of raw[2..4] = %ld\n", (long)sum16(raw + 2, 3));

    printf("\n== 5. byte buffer: split lines and verify checksum ==\n");
    /* UART 로 받은 바이트가 버퍼에 쌓여 있다고 가정합니다.
       첫 줄은 올바른 체크섬, 둘째 줄은 일부러 틀린 체크섬입니다. */
    const char rx[] = "$G*47\r\n$M,1*51\n$D,10";
    const char *cur = rx;
    size_t remain = strlen(rx);
    char line[49];
    size_t used;
    while ((used = take_line(cur, remain, line, sizeof(line))) > 0) {
        cur += used;
        remain -= used;

        const char *star = strchr(line, '*');
        if (line[0] != '$' || star == NULL) {
            printf("\"%s\" -> format error\n", line);
            continue;
        }
        uint8_t calc = xor_checksum(line + 1, star);
        unsigned given = 0;
        sscanf(star + 1, "%2X", &given);
        printf("\"%s\" -> calc=%02X given=%02X %s\n", line, (unsigned)calc, given,
               (calc == given) ? "OK" : "CHECKSUM ERROR (#E,1)");
    }
    printf("left in buffer (no newline yet): \"%s\" (%u bytes)\n", cur, (unsigned)remain);

    return 0;
}

/*
 * [연습 문제]
 *  1. sum16 을 인덱스 없이 포인터 증가(p++)만으로 다시 작성해 보세요.
 *  2. "$M,1*51" 의 올바른 체크섬은 50 입니다 (SPEC 4장 예시).
 *     rx 문자열을 고쳐서 두 줄 모두 OK 가 나오게 해 보세요.
 *  3. 버퍼에서 특정 바이트 c 의 첫 위치를 포인터로 돌려주는
 *     const uint8_t *find_byte(const uint8_t *buf, size_t n, uint8_t c) 를 작성해 보세요
 *     (없으면 NULL).
 */
