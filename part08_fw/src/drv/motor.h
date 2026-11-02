/* motor.h : L298N 2채널 모터 드라이버 (부호 있는 PWM 입력) */
#ifndef MOTOR_H
#define MOTOR_H
#include <stdint.h>

void    motor_init(void);
/* -255..255, + 는 로봇 전진 방향 */
void    motor_set(int16_t left, int16_t right);
/* 데드존 보상 매핑 (PC 테스트에서도 쓰기 위해 공개) */
uint8_t motor_map_duty(int16_t cmd);

#endif
