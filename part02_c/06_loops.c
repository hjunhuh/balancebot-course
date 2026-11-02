/*
 * 06_loops.c : 반복문 for / while / do while, break, continue
 *
 * [학습 목표]
 *  1. for, while, do while 의 차이와 쓰임새를 설명할 수 있습니다.
 *  2. break 와 continue 로 반복 흐름을 제어합니다.
 *  3. 임베디드 main 의 "슈퍼 루프" 구조(for (;;))를 이해합니다.
 *  4. 정수 자료형 크기 때문에 끝나지 않는 루프가 생기는 함정을 압니다.
 */

#include <stdio.h>
#include <stdint.h>

int main(void)
{
    printf("== 1. for: fixed count ==\n");
    /* for (초기식; 조건식; 증감식) */
    for (int i = 0; i < 5; i++) {
        printf("%d ", i);
    }
    printf("\n");

    printf("\n== 2. while: until condition changes ==\n");
    /* 반복 횟수를 미리 모를 때 씁니다. 예: 값을 2로 계속 나누기 */
    unsigned v = 1000;
    int steps = 0;
    while (v > 1) {
        v /= 2;
        steps++;
    }
    printf("1000 halved %d times -> %u\n", steps, v);

    printf("\n== 3. do while: run at least once ==\n");
    /* 조건을 나중에 검사하므로 최소 한 번은 실행됩니다.
       센서에 "준비됐니?" 를 물어보고 준비될 때까지 다시 묻는 모양에 어울립니다. */
    int tries = 0;
    int ready;
    do {
        tries++;
        ready = (tries >= 3);   /* 세 번째에 준비 완료라고 가정 */
        printf("poll #%d ready=%d\n", tries, ready);
    } while (!ready);

    printf("\n== 4. break and continue ==\n");
    /* 데이터에서 음수(오류값)는 건너뛰고, 999 를 만나면 중단합니다. */
    const int data[] = { 12, -1, 30, 7, -1, 999, 50 };
    int sum = 0;
    for (int i = 0; i < 7; i++) {
        if (data[i] < 0) {
            printf("skip index %d\n", i);
            continue;           /* 아래를 건너뛰고 다음 반복으로 */
        }
        if (data[i] == 999) {
            printf("stop at index %d\n", i);
            break;              /* 반복문 탈출 */
        }
        sum += data[i];
    }
    printf("sum = %d\n", sum);

    printf("\n== 5. nested loop ==\n");
    for (int row = 1; row <= 3; row++) {
        for (int col = 1; col <= 4; col++) {
            printf("%3d", row * col);
        }
        printf("\n");
    }

    printf("\n== 6. super loop (embedded main) ==\n");
    /* 실제 펌웨어의 main 은 대략 다음 모양입니다.
     *
     *   int main(void) {
     *       init_all();
     *       for (;;) {                  // 영원히 반복
     *           if (tick_5ms) { ... }   // 제어 주기 처리
     *           uart_poll();            // 통신 처리
     *       }
     *   }
     *
     * PC 에서는 끝나야 하므로 10 틱만 돌고 빠져나옵니다. */
    uint32_t tick = 0;
    for (;;) {
        tick++;
        if (tick % 5 == 0) {
            printf("tick %u: control step\n", (unsigned)tick);
        }
        if (tick >= 10) {
            break;
        }
    }

    printf("\n== 7. infinite loop trap ==\n");
    /* for (uint8_t i = 0; i < 256; i++) 는 끝나지 않습니다.
       uint8_t 는 255 다음에 0 으로 돌아가므로 i < 256 이 항상 참입니다.
       (-Wextra 에서 "comparison is always true" 경고가 납니다.)
       아래는 안전하게 고친 예: 더 넓은 자료형으로 셉니다. */
    unsigned count = 0;
    for (uint16_t i = 0; i < 256; i++) {
        count++;
    }
    printf("loop ran %u times\n", count);

    /* 거꾸로 세기에서도 부호 없는 정수는 조심해야 합니다.
       for (unsigned i = 4; i >= 0; i--) 는 끝나지 않습니다. 대신 이렇게 씁니다. */
    for (unsigned i = 5; i-- > 0; ) {
        printf("%u ", i);
    }
    printf("\n");

    return 0;
}

/*
 * [연습 문제]
 *  1. 1부터 100까지 중 3의 배수이면서 5의 배수가 아닌 수의 합을 for 와 continue 로
 *     구해 보세요.
 *  2. while 문으로 정수 n 의 10진 자릿수를 세는 코드를 작성해 보세요 (n=0 이면 1자리).
 *  3. 6번 슈퍼 루프에서 tick 이 3의 배수일 때 "uart poll" 을 출력하도록 추가하고,
 *     두 작업이 같은 틱에 겹치는 경우를 찾아보세요.
 */
