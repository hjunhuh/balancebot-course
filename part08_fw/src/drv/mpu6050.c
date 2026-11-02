/*
 * mpu6050.c
 * 레지스터
 *   0x6B PWR_MGMT_1   0x01 : 슬립 해제, 클럭 = X 자이로 PLL (내부 8 MHz 보다 안정)
 *   0x19 SMPLRT_DIV   4    : DLPF 사용 시 1 kHz / (1 + 4) = 200 Hz (제어 주기와 같음)
 *   0x1A CONFIG       3    : DLPF 가속도 44 Hz, 자이로 42 Hz (모터 진동 걸러냄)
 *   0x1B GYRO_CONFIG  0x00 : ±250 deg/s
 *   0x1C ACCEL_CONFIG 0x00 : ±2 g
 *   0x3B ACCEL_XOUT_H 부터 14 바이트: AX AY AZ TEMP GX GY GZ (각각 상위 바이트 먼저)
 *   0x75 WHO_AM_I     0x68 (호환 칩은 0x70, 0x72, 0x98 을 돌려주기도 함)
 */
#include <util/delay.h>
#include "mpu6050.h"
#include "bsw_i2c.h"

static int16_t be16(const uint8_t *b)
{
    return (int16_t)(((uint16_t)b[0] << 8) | b[1]);
}

uint8_t mpu6050_init(void)
{
    uint8_t who = 0;
    if (!bsw_i2c_read_regs(MPU6050_ADDR, 0x75, &who, 1)) return 0;
    if (who != 0x68u && who != 0x70u && who != 0x72u && who != 0x98u) return 0;

    if (!bsw_i2c_write_reg(MPU6050_ADDR, 0x6B, 0x80)) return 0;   /* 리셋 */
    _delay_ms(100);
    if (!bsw_i2c_write_reg(MPU6050_ADDR, 0x6B, 0x01)) return 0;
    if (!bsw_i2c_write_reg(MPU6050_ADDR, 0x19, 4))    return 0;
    if (!bsw_i2c_write_reg(MPU6050_ADDR, 0x1A, 3))    return 0;
    if (!bsw_i2c_write_reg(MPU6050_ADDR, 0x1B, 0x00)) return 0;
    if (!bsw_i2c_write_reg(MPU6050_ADDR, 0x1C, 0x00)) return 0;
    _delay_ms(50);
    return 1;
}

uint8_t mpu6050_read(mpu6050_raw_t *r)
{
    uint8_t b[14];
    if (!bsw_i2c_read_regs(MPU6050_ADDR, 0x3B, b, 14)) return 0;
    r->ax   = be16(&b[0]);
    r->ay   = be16(&b[2]);
    r->az   = be16(&b[4]);
    r->temp = be16(&b[6]);
    r->gx   = be16(&b[8]);
    r->gy   = be16(&b[10]);
    r->gz   = be16(&b[12]);
    return 1;
}
