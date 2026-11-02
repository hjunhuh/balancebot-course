/* mpu6050.h : MPU6050 디바이스 드라이버 (bsw_i2c 사용) */
#ifndef MPU6050_H
#define MPU6050_H
#include <stdint.h>

#define MPU6050_ADDR   0x68u

typedef struct {
    int16_t ax, ay, az;     /* 16384 LSB/g  (±2 g)     */
    int16_t temp;
    int16_t gx, gy, gz;     /* 131 LSB/(deg/s) (±250) */
} mpu6050_raw_t;

uint8_t mpu6050_init(void);                 /* 1 성공 (WHO_AM_I 확인 포함) */
uint8_t mpu6050_read(mpu6050_raw_t *r);     /* 1 성공, 14 바이트 버스트 읽기 */

#endif
