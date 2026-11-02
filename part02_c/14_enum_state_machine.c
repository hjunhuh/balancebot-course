/*
 * 14_enum_state_machine.c : enum 과 상태 머신
 *
 * [학습 목표]
 *  1. enum 으로 의미 있는 이름을 가진 정수 상수 묶음을 만듭니다.
 *  2. switch 와 enum 으로 상태 머신(state machine)을 구현합니다.
 *  3. 상태 진입 시 한 번만 하는 일(entry action)과 매 주기 하는 일을 구분합니다.
 *  4. 밸런싱 로봇의 상태 흐름 INIT -> CALIB -> WAIT_UPRIGHT -> BALANCE <-> FALLEN 을
 *     PC 에서 가짜 센서 값으로 흉내 냅니다.
 */

#include <stdio.h>
#include <stdint.h>

/* 상태 번호는 SPEC 4장 프로토콜의 상태 번호와 같게 맞춥니다.
   값을 명시해 두면 텔레메트리(#A 의 st 필드)로 그대로 보낼 수 있습니다. */
typedef enum {
    ST_INIT         = 0,
    ST_CALIB        = 1,
    ST_WAIT_UPRIGHT = 2,
    ST_BALANCE      = 3,
    ST_FALLEN       = 4,
    ST_IDLE         = 5,
    ST_LOW_BATT     = 6,
    ST_COUNT              /* 상태 개수 (7). 배열 크기로 쓰기 좋습니다 */
} RobotState;

/* enum 값과 같은 순서의 이름 표 */
static const char *const STATE_NAME[ST_COUNT] = {
    "INIT", "CALIB", "WAIT_UPRIGHT", "BALANCE", "FALLEN", "IDLE", "LOW_BATT"
};

#define CALIB_SAMPLES     3      /* 실제로는 수백 개를 평균합니다 */
#define UPRIGHT_X100    500      /* |각도| < 5 도 면 섰다고 봅니다 */
#define FALL_X100      4500      /* |각도| > 45 도 면 넘어졌다고 봅니다 */
#define LOW_BATT_MV    6400

/* 상태 머신이 쓰는 데이터 묶음 */
typedef struct {
    RobotState state;
    uint16_t   ticks_in_state;   /* 현재 상태에 머문 주기 수 */
    int        calib_count;
    int32_t    gyro_sum;
    int16_t    gyro_offset;
} Robot;

static int iabs(int v) { return v < 0 ? -v : v; }

/* 상태를 바꾸는 유일한 함수: 로그를 남기고 카운터를 초기화합니다.
   상태를 여기서만 바꾸면 디버깅이 쉬워집니다. */
static void change_state(Robot *r, RobotState next)
{
    printf("        state %s -> %s\n", STATE_NAME[r->state], STATE_NAME[next]);
    r->state = next;
    r->ticks_in_state = 0;
    /* 진입 동작 (entry action) */
    switch (next) {
    case ST_CALIB:
        r->calib_count = 0;
        r->gyro_sum = 0;
        break;
    case ST_BALANCE:
        printf("        (entry) reset PID integral, enable motors\n");
        break;
    case ST_FALLEN:
    case ST_LOW_BATT:
        printf("        (entry) motors OFF\n");
        break;
    default:
        break;
    }
}

/* 5 ms 마다 한 번 호출된다고 가정하는 한 스텝 */
static void robot_step(Robot *r, int angle_x100, int16_t gyro_raw, uint16_t vbat_mv)
{
    /* 어느 상태에서든 배터리가 낮으면 우선 처리합니다 */
    if (vbat_mv < LOW_BATT_MV && r->state != ST_LOW_BATT) {
        change_state(r, ST_LOW_BATT);
        return;
    }

    switch (r->state) {
    case ST_INIT:
        /* 주변장치 초기화가 끝났다고 보고 바로 보정으로 넘어갑니다 */
        change_state(r, ST_CALIB);
        break;

    case ST_CALIB:
        r->gyro_sum += gyro_raw;
        r->calib_count++;
        if (r->calib_count >= CALIB_SAMPLES) {
            r->gyro_offset = (int16_t)(r->gyro_sum / r->calib_count);
            printf("        gyro offset = %d\n", r->gyro_offset);
            change_state(r, ST_WAIT_UPRIGHT);
        }
        break;

    case ST_WAIT_UPRIGHT:
        if (iabs(angle_x100) < UPRIGHT_X100) {
            change_state(r, ST_BALANCE);
        }
        break;

    case ST_BALANCE:
        if (iabs(angle_x100) > FALL_X100) {
            change_state(r, ST_FALLEN);
        }
        break;

    case ST_FALLEN:
        /* 다시 세워 주면 균형 제어를 재개합니다 */
        if (iabs(angle_x100) < UPRIGHT_X100) {
            change_state(r, ST_BALANCE);
        }
        break;

    case ST_IDLE:
    case ST_LOW_BATT:
        /* 아무것도 하지 않습니다 (빠져나가려면 재시작) */
        break;

    case ST_COUNT:
    default:
        /* 여기 오면 메모리가 깨진 것입니다. 안전한 상태로 보냅니다. */
        change_state(r, ST_FALLEN);
        break;
    }
    r->ticks_in_state++;
}

int main(void)
{
    printf("sizeof(RobotState) = %u, ST_COUNT = %d\n\n", (unsigned)sizeof(RobotState), ST_COUNT);

    /* 가짜 입력 시나리오: {각도 x100, 자이로 원시값, 배터리 mV} */
    const struct { int angle; int16_t gyro; uint16_t mv; } input[] = {
        {  3000, 12, 8200 },   /* 0: 누워 있는 상태로 전원 ON */
        {  3000, 14, 8200 },   /* 1 */
        {  3000, 10, 8200 },   /* 2 */
        {  3000, 12, 8190 },   /* 3: 보정 끝 */
        {  1200,  0, 8190 },   /* 4: 세우는 중 */
        {   300,  0, 8180 },   /* 5: 섰음 -> BALANCE */
        {  -150,  0, 8180 },   /* 6 */
        {  2000,  0, 8170 },   /* 7: 크게 기울어짐 */
        {  5200,  0, 8170 },   /* 8: 넘어짐 -> FALLEN */
        {  6000,  0, 8160 },   /* 9 */
        {   100,  0, 8160 },   /* 10: 다시 세움 -> BALANCE */
        {    50,  0, 6300 },   /* 11: 배터리 저전압 -> LOW_BATT */
        {    50,  0, 6300 },   /* 12 */
    };
    const int n = (int)(sizeof(input) / sizeof(input[0]));

    Robot robot = { .state = ST_INIT };
    for (int t = 0; t < n; t++) {
        printf("t=%2d angle=%5d mv=%u [%s]\n", t, input[t].angle,
               (unsigned)input[t].mv, STATE_NAME[robot.state]);
        robot_step(&robot, input[t].angle, input[t].gyro, input[t].mv);
    }
    printf("\nfinal state = %s (telemetry st=%d)\n", STATE_NAME[robot.state], (int)robot.state);
    return 0;
}

/*
 * [연습 문제]
 *  1. FALLEN 상태에서 바로 BALANCE 로 가지 않고, 섰다는 조건이 200 주기(1초) 동안
 *     계속 유지될 때만 넘어가도록 ticks_in_state 를 활용해 고쳐 보세요.
 *  2. '$M,0' 명령을 받으면 IDLE 로, '$M,1' 을 받으면 WAIT_UPRIGHT 로 가는
 *     함수 robot_motor_cmd(Robot *r, int enable) 를 추가해 보세요.
 *  3. switch 에서 case ST_IDLE 줄을 지우고 -Wall 로 빌드하면 어떤 경고가 나오나요?
 *     (default 가 있을 때와 없을 때를 비교해 보세요.)
 */
