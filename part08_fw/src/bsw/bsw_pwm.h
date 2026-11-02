/* bsw_pwm.h : Timer0 Fast PWM, OC0A(D6)=왼쪽 ENA, OC0B(D5)=오른쪽 ENB */
#ifndef BSW_PWM_H
#define BSW_PWM_H
#include <stdint.h>

void bsw_pwm_init(void);
void bsw_pwm_set_a(uint8_t duty);    /* 0..255 */
void bsw_pwm_set_b(uint8_t duty);

#endif
