/* bsw_i2c.h : TWI 마스터 (폴링, 타임아웃 포함) 400 kHz */
#ifndef BSW_I2C_H
#define BSW_I2C_H
#include <stdint.h>

void    bsw_i2c_init(void);
uint8_t bsw_i2c_write_reg(uint8_t addr7, uint8_t reg, uint8_t val);               /* 1 성공 */
uint8_t bsw_i2c_read_regs(uint8_t addr7, uint8_t reg, uint8_t *buf, uint8_t n);   /* 1 성공 */

#endif
