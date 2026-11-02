/* bsw_gpio.h : HSI 핀맵 기반 GPIO 초기화와 단순 입출력 */
#ifndef BSW_GPIO_H
#define BSW_GPIO_H
#include <stdint.h>

void    bsw_gpio_init(void);
void    bsw_gpio_led(uint8_t on);
void    bsw_gpio_led_toggle(void);
uint8_t bsw_gpio_button_pressed(void);     /* 눌림 = 1 (풀업이라 LOW 가 눌림) */

/* L298N 방향 핀 */
void    bsw_gpio_motor_l_dir(uint8_t in1, uint8_t in2);
void    bsw_gpio_motor_r_dir(uint8_t in3, uint8_t in4);

#endif
