/* stub_bsw.c : PC 시뮬레이션용 BSW 대역 (레지스터 대신 아무 일도 하지 않음) */
#include <stdint.h>
#include "bsw_gpio.h"
#include "bsw_pwm.h"
void bsw_gpio_motor_l_dir(uint8_t a, uint8_t b) { (void)a; (void)b; }
void bsw_gpio_motor_r_dir(uint8_t a, uint8_t b) { (void)a; (void)b; }
void bsw_pwm_set_a(uint8_t d) { (void)d; }
void bsw_pwm_set_b(uint8_t d) { (void)d; }
