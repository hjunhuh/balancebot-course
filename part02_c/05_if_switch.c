/*
 * 05_if_switch.c : 조건문 if / else if / switch
 *
 * [학습 목표]
 *  1. if, else if, else 로 범위 판단(배터리 상태 등)을 작성할 수 있습니다.
 *  2. switch 와 case, break, default 의 동작과 fall through 를 이해합니다.
 *  3. 한 글자 명령 문자를 switch 로 분기하는 방법을 익힙니다
 *     (프로토콜 v1 의 'D', 'P', 'T', 'M', 'S', 'G' 처리와 같은 모양).
 *  4. = 와 == 혼동, 중괄호 생략 같은 흔한 실수를 피합니다.
 */

#include <stdio.h>
#include <stdint.h>

/* 2S 리튬이온 배터리 전압(mV)을 상태 문자열로 바꿉니다.
   위에서부터 차례로 검사하므로 조건의 순서가 중요합니다. */
static const char *battery_level(uint16_t mv)
{
    if (mv >= 8000) {
        return "FULL";
    } else if (mv >= 7400) {
        return "GOOD";
    } else if (mv >= 6800) {
        return "LOW";
    } else {
        return "CRITICAL";     /* 펌웨어에서는 LOW_BATT 상태로 들어갑니다 */
    }
}

/* 명령 문자 하나를 받아 설명을 출력합니다. */
static void handle_command(char cmd)
{
    switch (cmd) {
    case 'D':
        printf("'%c' -> joystick\n", cmd);
        break;              /* break 가 없으면 아래 case 로 계속 내려갑니다 */
    case 'P':
        printf("'%c' -> PID gains\n", cmd);
        break;
    case 'T':
        printf("'%c' -> trim angle\n", cmd);
        break;
    case 'M':
        printf("'%c' -> motor enable\n", cmd);
        break;
    case 's':               /* 소문자도 같은 처리를 하고 싶을 때: 의도적 fall through */
    case 'S':
        printf("'%c' -> save to EEPROM\n", cmd);
        break;
    case 'G':
        printf("'%c' -> get parameters\n", cmd);
        break;
    default:                /* 어느 case 에도 맞지 않을 때 */
        printf("'%c' -> unknown command (#E,4)\n", cmd);
        break;
    }
}

/* break 를 빠뜨렸을 때 무슨 일이 생기는지 보여 줍니다. */
static void fallthrough_demo(int n)
{
    printf("n=%d :", n);
    switch (n) {
    case 1:
        printf(" one");
        /* fall through */  /* 주석으로 의도를 밝히면 gcc 의 경고도 사라집니다 */
    case 2:
        printf(" two");
        /* fall through */
    case 3:
        printf(" three");
        break;
    default:
        printf(" other");
        break;
    }
    printf("\n");
}

int main(void)
{
    printf("== 1. if / else if / else ==\n");
    const uint16_t samples[] = { 8350, 7600, 7000, 6500 };
    for (int i = 0; i < 4; i++) {
        printf("%u mV -> %s\n", (unsigned)samples[i], battery_level(samples[i]));
    }

    printf("\n== 2. nested condition: is robot fallen? ==\n");
    int angle_x100 = -4200;    /* -42.00 도 */
    int abs_angle = (angle_x100 < 0) ? -angle_x100 : angle_x100;
    if (abs_angle > 4500) {
        printf("angle %d -> FALLEN\n", angle_x100);
    } else if (abs_angle > 3500) {
        printf("angle %d -> WARNING (close to falling)\n", angle_x100);
    } else {
        printf("angle %d -> OK\n", angle_x100);
    }

    printf("\n== 3. switch on command character ==\n");
    const char cmds[] = { 'D', 'P', 'S', 's', 'G', 'X' };
    for (int i = 0; i < (int)sizeof(cmds); i++) {
        handle_command(cmds[i]);
    }

    printf("\n== 4. fall through ==\n");
    for (int n = 1; n <= 4; n++) {
        fallthrough_demo(n);
    }

    printf("\n== 5. = vs == ==\n");
    int mode = 0;
    /* if (mode = 1) 이라고 쓰면 대입이 일어나고 항상 참이 됩니다.
       -Wall 에서 "suggest parentheses around assignment" 경고가 나옵니다.
       상수를 왼쪽에 쓰는 (1 == mode) 습관도 실수를 막는 방법 중 하나입니다. */
    if (mode == 1) {
        printf("mode is 1\n");
    } else {
        printf("mode is not 1 (mode=%d)\n", mode);
    }

    return 0;
}

/*
 * [연습 문제]
 *  1. battery_level 에서 조건의 순서를 거꾸로(6800 먼저) 바꾸면 8350 mV 가
 *     어떤 결과로 나오는지 예측하고 확인해 보세요.
 *  2. handle_command 에 'V' (속도 보상 게인) case 를 추가해 보세요.
 *  3. 각도 절댓값이 4500 을 넘으면 4, 아니면 3 을 돌려주는 함수를
 *     삼항 연산자 한 줄로 작성해 보세요 (상태 번호 FALLEN = 4, BALANCE = 3).
 */
