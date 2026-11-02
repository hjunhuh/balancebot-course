/*
 * lab07_motor_l298n: Timer0 PWM + 방향핀으로 L298N 모터 2개 구동, 가감속 램프, 데드존 보상
 *
 * 배선 (SPEC 핀맵):
 *   D6 (PD6, OC0A) -> ENA  (왼쪽 속도)      ENA/ENB 의 점퍼 캡은 반드시 빼고 연결합니다
 *   D7 (PD7)       -> IN1  (왼쪽 방향)
 *   D8 (PB0)       -> IN2  (왼쪽 방향)
 *   D5 (PD5, OC0B) -> ENB  (오른쪽 속도)
 *   D4 (PD4)       -> IN3  (오른쪽 방향)
 *   D9 (PB1)       -> IN4  (오른쪽 방향)
 *   L298N GND 와 Uno GND 를 반드시 공통으로 연결합니다.
 *   버튼 D3 (PD3): 누르고 있으면 즉시 정지 (비상 정지 연습)
 *   상태 LED D10 (PB2): 모터가 움직이는 중에는 켜짐
 *
 * 주의: 실습 중에는 바퀴를 바닥에서 띄워 둡니다.
 *
 * 방향 규칙: IN1=1, IN2=0 이면 왼쪽 정방향, IN3=1, IN4=0 이면 오른쪽 정방향.
 * 반대로 돌면 MOTOR_L_INVERT / MOTOR_R_INVERT 를 1 로 바꿉니다.
 */
#include <avr/io.h>
#include <util/delay.h>
#include <stdlib.h>

#define MOTOR_L_INVERT  0
#define MOTOR_R_INVERT  0

/*
 * 데드존: 듀티가 너무 작으면 L298N 의 전압 강하(약 2 V)와 기어 마찰 때문에
 * 모터가 "윙" 소리만 내고 돌지 않습니다. 이 구간을 건너뛰도록
 * 명령 1..255 를 DEADZONE..255 로 다시 매핑합니다.
 * 값은 바퀴를 띄운 상태에서 막 돌기 시작하는 OCR 값을 직접 찾아 넣습니다.
 */
#define MOTOR_DEADZONE  40

#define RAMP_STEP       5      /* 램프 주기(10 ms)마다 명령이 바뀔 수 있는 최대 크기 */
#define RAMP_PERIOD_MS  10     /* 0 -> 255 까지 255/5 x 10 ms = 약 0.5 s */

static void motor_init(void)
{
    /* 방향핀 출력, 초기값 0 (L298N: EN=0 이면 프리런 정지) */
    DDRD  |= (1 << PD4) | (1 << PD5) | (1 << PD6) | (1 << PD7);
    DDRB  |= (1 << PB0) | (1 << PB1);
    PORTD &= (uint8_t)~((1 << PD4) | (1 << PD5) | (1 << PD6) | (1 << PD7));
    PORTB &= (uint8_t)~((1 << PB0) | (1 << PB1));

    /* Timer0: Fast PWM 8비트, 분주 8, 7812.5 Hz. COM 비트는 motor_set 에서 */
    OCR0A  = 0;
    OCR0B  = 0;
    TCCR0A = (1 << WGM01) | (1 << WGM00);
    TCCR0B = (1 << CS01);
}

/* 0..255 크기를 데드존 보상한 OCR 값으로. 0 은 0 그대로 */
static uint8_t deadzone_map(uint8_t mag)
{
    if (mag == 0) return 0;
    return (uint8_t)(MOTOR_DEADZONE + ((uint16_t)mag * (255 - MOTOR_DEADZONE) + 127) / 255);
}

/* 왼쪽 모터: cmd -255..255 (+ 정방향) */
static void motor_left(int16_t cmd)
{
    if (MOTOR_L_INVERT) cmd = (int16_t)-cmd;
    if (cmd > 255) cmd = 255;
    if (cmd < -255) cmd = -255;
    uint8_t duty = deadzone_map((uint8_t)abs(cmd));

    if (cmd > 0)      { PORTD |= (1 << PD7);  PORTB &= (uint8_t)~(1 << PB0); }   /* IN1=1 IN2=0 */
    else if (cmd < 0) { PORTD &= (uint8_t)~(1 << PD7); PORTB |= (1 << PB0); }    /* IN1=0 IN2=1 */
    else              { PORTD &= (uint8_t)~(1 << PD7); PORTB &= (uint8_t)~(1 << PB0); }

    if (duty == 0) {
        TCCR0A &= (uint8_t)~((1 << COM0A1) | (1 << COM0A0));  /* ENA = 0 고정 */
    } else {
        OCR0A   = duty;
        TCCR0A |= (1 << COM0A1);
    }
}

/* 오른쪽 모터: cmd -255..255 (+ 정방향) */
static void motor_right(int16_t cmd)
{
    if (MOTOR_R_INVERT) cmd = (int16_t)-cmd;
    if (cmd > 255) cmd = 255;
    if (cmd < -255) cmd = -255;
    uint8_t duty = deadzone_map((uint8_t)abs(cmd));

    if (cmd > 0)      { PORTD |= (1 << PD4);  PORTB &= (uint8_t)~(1 << PB1); }   /* IN3=1 IN4=0 */
    else if (cmd < 0) { PORTD &= (uint8_t)~(1 << PD4); PORTB |= (1 << PB1); }    /* IN3=0 IN4=1 */
    else              { PORTD &= (uint8_t)~(1 << PD4); PORTB &= (uint8_t)~(1 << PB1); }

    if (duty == 0) {
        TCCR0A &= (uint8_t)~((1 << COM0B1) | (1 << COM0B0));  /* ENB = 0 고정 */
    } else {
        OCR0B   = duty;
        TCCR0A |= (1 << COM0B1);
    }
}

static uint8_t estop_pressed(void)
{
    return (PIND & (1 << PD3)) == 0;
}

/*
 * 현재 명령 cur 를 목표 target 쪽으로 RAMP_STEP 만큼만 움직입니다.
 * 급격한 명령 변화는 큰 돌입 전류, 전원 전압 강하(MCU 리셋), 기어 충격을 만듭니다.
 */
static int16_t ramp_toward(int16_t cur, int16_t target)
{
    if (cur < target) { cur += RAMP_STEP; if (cur > target) cur = target; }
    else if (cur > target) { cur -= RAMP_STEP; if (cur < target) cur = target; }
    return cur;
}

int main(void)
{
    motor_init();
    DDRB  |= (1 << PB2);                 /* 상태 LED */
    DDRD  &= (uint8_t)~(1 << PD3);
    PORTD |= (1 << PD3);                 /* 버튼 풀업 */

    /* 데모 시나리오: {왼쪽 목표, 오른쪽 목표, 유지 시간 x 10 ms} */
    static const int16_t plan[][3] = {
        {  200,  200, 200 },   /* 전진 */
        {    0,    0, 100 },   /* 정지 */
        { -200, -200, 200 },   /* 후진 */
        {    0,    0, 100 },
        {  150, -150, 150 },   /* 제자리 오른쪽 회전 */
        {    0,    0, 100 },
        {   60,   60, 300 },   /* 저속: 데드존 보상 효과 확인 */
        {    0,    0, 200 },
    };
    const uint8_t n = sizeof(plan) / sizeof(plan[0]);

    int16_t cur_l = 0, cur_r = 0;

    for (;;) {
        for (uint8_t s = 0; s < n; s++) {
            for (uint16_t t = 0; t < (uint16_t)plan[s][2]; t++) {
                if (estop_pressed()) {
                    cur_l = 0;                       /* 비상 정지는 램프 없이 즉시 */
                    cur_r = 0;
                } else {
                    cur_l = ramp_toward(cur_l, plan[s][0]);
                    cur_r = ramp_toward(cur_r, plan[s][1]);
                }
                motor_left(cur_l);
                motor_right(cur_r);

                if (cur_l || cur_r) PORTB |= (1 << PB2);
                else                PORTB &= (uint8_t)~(1 << PB2);

                _delay_ms(RAMP_PERIOD_MS);
            }
        }
    }
}
