/*
 * 08_arrays.c : 배열과 이동평균 필터
 *
 * [학습 목표]
 *  1. 배열 선언, 초기화, 인덱스 접근, 요소 개수 계산(sizeof 활용)을 익힙니다.
 *  2. 배열 범위를 벗어난 접근이 왜 위험한지 이해합니다.
 *  3. 2차원 배열을 표(룩업 테이블)로 사용합니다.
 *  4. 원형 인덱스와 누적 합을 이용한 이동평균 필터를 구현합니다.
 *     (배터리 ADC 값이나 센서 노이즈를 부드럽게 만들 때 씁니다.)
 */

#include <stdio.h>
#include <stdint.h>

/* 배열의 요소 개수를 구하는 관용 매크로입니다.
   배열 이름에만 쓸 수 있고, 포인터에 쓰면 틀린 값이 나옵니다 (11장 참고). */
#define ARRAY_LEN(a) (sizeof(a) / sizeof((a)[0]))

/* ---------- 이동평균 필터 ----------
 * 최근 N 개 샘플의 평균을 냅니다. 매번 N 개를 다시 더하지 않고
 * "새 값을 더하고 가장 오래된 값을 빼는" 방식으로 누적 합을 갱신하면
 * 샘플 하나당 덧셈 한 번, 뺄셈 한 번이면 됩니다. */
#define MA_SIZE 4

static int16_t  ma_buf[MA_SIZE];   /* 전역 배열은 0 으로 자동 초기화됩니다 */
static uint8_t  ma_index = 0;      /* 다음에 쓸 위치 */
static uint8_t  ma_count = 0;      /* 지금까지 들어온 샘플 수 (최대 MA_SIZE) */
static int32_t  ma_sum   = 0;      /* 누적 합: 넘치지 않게 32비트 */

static int16_t moving_average(int16_t sample)
{
    ma_sum -= ma_buf[ma_index];        /* 가장 오래된 값을 빼고 */
    ma_buf[ma_index] = sample;         /* 그 자리에 새 값을 넣고 */
    ma_sum += sample;                  /* 새 값을 더합니다 */

    ma_index++;
    if (ma_index >= MA_SIZE) {         /* 끝에 닿으면 처음으로 (원형 인덱스) */
        ma_index = 0;
    }
    if (ma_count < MA_SIZE) {
        ma_count++;
    }
    return (int16_t)(ma_sum / ma_count);  /* 처음 몇 개는 들어온 개수로 나눕니다 */
}

int main(void)
{
    printf("== 1. declare and initialize ==\n");
    int a[5] = { 10, 20, 30 };   /* 나머지 두 개는 0 으로 채워집니다 */
    for (size_t i = 0; i < ARRAY_LEN(a); i++) {
        printf("a[%u] = %d\n", (unsigned)i, a[i]);
    }
    printf("sizeof(a) = %u bytes, length = %u\n", (unsigned)sizeof(a), (unsigned)ARRAY_LEN(a));

    printf("\n== 2. min / max / average ==\n");
    const int16_t gyro[] = { 12, -5, 30, 8, -20, 15, 3, 0 };
    int16_t mn = gyro[0], mx = gyro[0];
    int32_t total = 0;
    for (size_t i = 0; i < ARRAY_LEN(gyro); i++) {
        if (gyro[i] < mn) mn = gyro[i];
        if (gyro[i] > mx) mx = gyro[i];
        total += gyro[i];
    }
    printf("min=%d max=%d avg=%ld\n", mn, mx, (long)(total / (int32_t)ARRAY_LEN(gyro)));

    printf("\n== 3. 2D array as lookup table ==\n");
    /* [상태][항목] 표: 각 상태에서 LED 깜빡임 주기(ms)와 모터 허용 여부 */
    const uint16_t table[3][2] = {
        /* period_ms, motor_on */
        {  100, 0 },   /* CALIB   */
        {  500, 1 },   /* BALANCE */
        { 1000, 0 },   /* FALLEN  */
    };
    const char *names[3] = { "CALIB", "BALANCE", "FALLEN" };
    for (int s = 0; s < 3; s++) {
        printf("%-8s led=%4u ms motor=%u\n", names[s],
               (unsigned)table[s][0], (unsigned)table[s][1]);
    }

    printf("\n== 4. moving average filter (N=%d) ==\n", MA_SIZE);
    /* 노이즈가 섞인 ADC 값이라고 가정합니다. 중간에 튀는 값(900)이 있습니다. */
    const int16_t adc[] = { 500, 510, 490, 505, 900, 495, 500, 508, 492, 500 };
    printf("raw  : ");
    for (size_t i = 0; i < ARRAY_LEN(adc); i++) printf("%4d", adc[i]);
    printf("\nfilt : ");
    for (size_t i = 0; i < ARRAY_LEN(adc); i++) printf("%4d", moving_average(adc[i]));
    printf("\n");
    /* 튀는 값의 영향이 1/N 로 줄어들고 N 샘플 동안 남아 있는 것을 볼 수 있습니다.
       N 이 클수록 부드럽지만 반응이 늦어집니다 (지연). 균형 제어의 각도처럼
       빨리 반응해야 하는 값에는 쓰지 않고, 배터리 전압 같은 느린 값에 씁니다. */

    printf("\n== 5. out of bounds (do not do this) ==\n");
    /* int a[5] 의 유효 인덱스는 0..4 입니다. a[5] 를 쓰면 옆 메모리를 덮어씁니다.
       C 는 범위를 검사하지 않으므로 컴파일도 되고 실행도 될 수 있지만 결과는 정의되지
       않습니다. 그래서 인덱스는 항상 직접 검사합니다. */
    size_t idx = 7;
    if (idx < ARRAY_LEN(a)) {
        printf("a[%u] = %d\n", (unsigned)idx, a[idx]);
    } else {
        printf("index %u rejected (length %u)\n", (unsigned)idx, (unsigned)ARRAY_LEN(a));
    }

    return 0;
}

/*
 * [연습 문제]
 *  1. MA_SIZE 를 8 로 바꾸고 같은 입력에 대해 출력이 어떻게 달라지는지 비교해 보세요.
 *     900 의 영향이 얼마나 작아지고, 얼마나 오래 남는지 관찰하세요.
 *  2. 배열을 거꾸로 뒤집는 코드(제자리 교환)를 작성해 gyro 배열에 적용해 보세요.
 *  3. 이동평균 대신 "최근 3개의 중앙값" 필터를 만들어 900 같은 튀는 값을
 *     완전히 제거할 수 있는지 확인해 보세요.
 */
