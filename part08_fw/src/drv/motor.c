/*
 * motor.c
 * DC 모터는 일정 전압 이하에서는 마찰 때문에 돌지 않습니다 (데드존).
 * 제어기 출력 1..255 를 DEADBAND..255 로 다시 펼쳐서, 작은 출력에도 바퀴가 반응하게 합니다.
 *   duty = DEADBAND + |cmd| * (255 - DEADBAND) / 255   (cmd != 0)
 */
#include "motor.h"
#include "config.h"
#include "bsw_gpio.h"
#include "bsw_pwm.h"

uint8_t motor_map_duty(int16_t cmd)
{
    int16_t mag = (cmd < 0) ? (int16_t)-cmd : cmd;
    if (mag == 0) return 0;
    if (mag > PWM_MAX) mag = PWM_MAX;
    uint16_t d = (uint16_t)MOTOR_DEADBAND_PWM
               + (uint16_t)(((uint32_t)mag * (uint32_t)(PWM_MAX - MOTOR_DEADBAND_PWM)) / (uint32_t)PWM_MAX);
    return (uint8_t)d;
}

void motor_init(void)
{
    motor_set(0, 0);
}

void motor_set(int16_t left, int16_t right)
{
#if MOTOR_L_INVERT
    left = (int16_t)-left;
#endif
#if MOTOR_R_INVERT
    right = (int16_t)-right;
#endif
    /* 왼쪽: ENA(OC0A), IN1, IN2 */
    if (left > 0)       bsw_gpio_motor_l_dir(1, 0);
    else if (left < 0)  bsw_gpio_motor_l_dir(0, 1);
    else                bsw_gpio_motor_l_dir(0, 0);     /* 자유 정지 */
    bsw_pwm_set_a(motor_map_duty(left));

    /* 오른쪽: ENB(OC0B), IN3, IN4 */
    if (right > 0)      bsw_gpio_motor_r_dir(1, 0);
    else if (right < 0) bsw_gpio_motor_r_dir(0, 1);
    else                bsw_gpio_motor_r_dir(0, 0);
    bsw_pwm_set_b(motor_map_duty(right));
}
