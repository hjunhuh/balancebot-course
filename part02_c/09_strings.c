/*
 * 09_strings.c : 문자열, 직접 구현한 정수 파싱, sprintf
 *
 * [학습 목표]
 *  1. C 문자열은 끝에 '\0' 이 붙은 char 배열이라는 것을 이해합니다.
 *  2. strlen, strcmp, strncpy 같은 기본 함수와 버퍼 크기 문제를 압니다.
 *  3. atoi 를 쓰지 않고 오류 검사가 되는 정수 파싱 함수를 직접 만듭니다
 *     (블루투스 프레임 "$P,1200,50,80" 의 필드를 읽는 데 그대로 쓰입니다).
 *  4. snprintf 로 숫자를 문자열로 만들어 전송 프레임을 조립합니다.
 */

#include <stdio.h>
#include <string.h>
#include <stdint.h>

/* ---------- 직접 만든 정수 파싱 ----------
 * 문자열 s 에서 10진 정수 하나를 읽어 *out 에 넣습니다.
 *   - 앞에 '-' 또는 '+' 가 올 수 있습니다.
 *   - 숫자가 하나도 없거나, int32_t 범위를 넘으면 실패(0)를 돌려줍니다.
 *   - 성공하면 숫자 다음 문자의 위치를 *end 에 알려 줍니다 (NULL 이면 무시).
 * atoi 는 "abc" 를 받아도 0 을 돌려주므로 "진짜 0" 과 오류를 구분할 수 없습니다.
 * 통신 데이터는 깨질 수 있으므로 반드시 오류를 구분하는 파서를 씁니다. */
static int parse_int32(const char *s, int32_t *out, const char **end)
{
    int negative = 0;
    int64_t value = 0;     /* 넘침 검사를 위해 더 넓은 자료형으로 계산합니다 */
    int digits = 0;

    if (*s == '-') { negative = 1; s++; }
    else if (*s == '+') { s++; }

    while (*s >= '0' && *s <= '9') {
        value = value * 10 + (*s - '0');   /* 문자 '7' - '0' = 숫자 7 */
        if (value > 2147483648LL) {        /* -2147483648 까지 허용 */
            return 0;
        }
        digits++;
        s++;
    }
    if (digits == 0) {
        return 0;
    }
    if (negative) value = -value;
    if (value > INT32_MAX || value < INT32_MIN) {
        return 0;
    }
    *out = (int32_t)value;
    if (end != NULL) {
        *end = s;
    }
    return 1;
}

int main(void)
{
    printf("== 1. a string is a char array ending with '\\0' ==\n");
    char name[] = "HC-05";      /* 실제 크기는 글자 5개 + '\0' = 6 */
    printf("text=\"%s\" strlen=%u sizeof=%u\n", name, (unsigned)strlen(name), (unsigned)sizeof(name));
    for (size_t i = 0; i < sizeof(name); i++) {
        if (name[i] != '\0') {
            printf("name[%u] = 0x%02X '%c'\n", (unsigned)i, (unsigned)(unsigned char)name[i], name[i]);
        } else {
            printf("name[%u] = 0x%02X (end mark)\n", (unsigned)i, (unsigned)(unsigned char)name[i]);
        }
    }

    printf("\n== 2. compare and copy ==\n");
    const char *cmd = "BALANCE";
    /* 문자열은 == 로 비교하면 주소를 비교하게 됩니다. 내용 비교는 strcmp 입니다. */
    printf("strcmp(cmd, \"BALANCE\") = %d\n", strcmp(cmd, "BALANCE"));
    printf("strcmp(\"A\", \"B\")       = %d (negative)\n", strcmp("A", "B") < 0 ? -1 : 1);

    char small[6];
    /* strncpy 는 원본이 길면 '\0' 을 붙이지 않으므로 마지막을 직접 막아 둡니다. */
    strncpy(small, "WAIT_UPRIGHT", sizeof(small) - 1);
    small[sizeof(small) - 1] = '\0';
    printf("copied into 6-byte buffer: \"%s\"\n", small);

    printf("\n== 3. parse integers by hand ==\n");
    const char *tests[] = { "1200", "-350", "+42", "0", "abc", "", "99999999999", "-2147483648" };
    for (size_t i = 0; i < sizeof(tests) / sizeof(tests[0]); i++) {
        int32_t v = 0;
        if (parse_int32(tests[i], &v, NULL)) {
            printf("\"%s\" -> OK %ld\n", tests[i], (long)v);
        } else {
            printf("\"%s\" -> ERROR\n", tests[i]);
        }
    }

    printf("\n== 4. parse comma separated fields ==\n");
    /* 프로토콜 v1 의 PID 프레임 본문입니다. "P," 다음부터 숫자 3개를 읽습니다. */
    const char *frame = "P,1200,50,80";
    const char *p = frame + 2;            /* "1200,50,80" 의 시작 */
    int32_t gains[3];
    int ok = 1;
    for (int i = 0; i < 3; i++) {
        if (!parse_int32(p, &gains[i], &p)) { ok = 0; break; }
        if (i < 2) {
            if (*p != ',') { ok = 0; break; }
            p++;                           /* 쉼표 건너뛰기 */
        }
    }
    if (ok && *p == '\0') {
        printf("kp=%ld ki=%ld kd=%ld\n", (long)gains[0], (long)gains[1], (long)gains[2]);
    } else {
        printf("format error (#E,2)\n");
    }

    printf("\n== 5. build a frame with snprintf ==\n");
    /* sprintf 는 버퍼 크기를 모르므로 넘칠 수 있습니다. 항상 snprintf 를 씁니다.
       반환값은 "버퍼가 충분했다면 썼을 글자 수" 입니다. */
    char out[48];
    int angle_x100 = -152, motor = 87, mv = 7820, state = 3;
    int n = snprintf(out, sizeof(out), "#A,%d,%d,%d,%d", angle_x100, motor, mv, state);
    printf("frame=\"%s\" length=%d\n", out, n);

    /* 버퍼가 작으면 snprintf 는 들어가는 만큼만 쓰고 끝에 '\0' 을 붙입니다.
       값이 상수이면 gcc 가 컴파일할 때 잘림을 미리 알아채고 -Wformat-truncation
       경고를 냅니다 (좋은 기능입니다). 실제로는 센서 값처럼 실행 중에 바뀌는 값을
       출력하므로 컴파일러가 미리 알 수 없습니다. 여기서는 그 상황을 흉내 내려고
       volatile 변수(16장에서 설명)에서 값을 읽습니다. */
    char tiny[8];
    volatile int live_angle = angle_x100, live_motor = motor;
    n = snprintf(tiny, sizeof(tiny), "#A,%d,%d", live_angle, live_motor);
    printf("tiny=\"%s\" wanted=%d (truncated: %s)\n", tiny, n,
           (n >= (int)sizeof(tiny)) ? "yes" : "no");

    printf("\n== 6. hex digits ==\n");
    /* 체크섬 "*47" 같은 16진 2자리를 만들 때 씁니다. */
    snprintf(out, sizeof(out), "%02X", 0x47u);
    printf("0x47 as text: \"%s\"\n", out);

    return 0;
}

/*
 * [연습 문제]
 *  1. 16진 문자 하나('0'..'9', 'A'..'F', 'a'..'f')를 0..15 로 바꾸는 함수
 *     hex_val(char c) 를 작성하고, 실패하면 -1 을 돌려주게 하세요.
 *  2. 4번 예제의 프레임을 "P,1200,,80" 이나 "P,1200,50,80,7" 로 바꾸면
 *     각각 어떤 결과가 나오는지 예측하고 확인해 보세요.
 *  3. 자기만의 itoa (정수를 문자열로) 함수를 snprintf 없이 작성해 보세요.
 *     힌트: 10으로 나눈 나머지를 거꾸로 쌓은 다음 뒤집습니다.
 */
