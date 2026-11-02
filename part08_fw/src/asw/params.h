/*
 * params.h
 * 튜닝 파라미터와 EEPROM 저장 형식 (직렬화, CRC8 검증).
 *
 * EEPROM 블롭 레이아웃 (리틀엔디언, 22 바이트, 주소 0 부터):
 *   0  magic   uint16  0xB0B1
 *   2  version uint8   1
 *   3  kp      int32   x100
 *   7  ki      int32   x100
 *  11  kd      int32   x100
 *  15  ks      int32   x100
 *  19  trim    int16   x100 [deg]
 *  21  crc8    uint8   (0..20 바이트, 다항식 0x07)
 */
#ifndef PARAMS_H
#define PARAMS_H

#include <stdint.h>

#define PARAMS_MAGIC       0xB0B1u
#define PARAMS_VERSION     1u
#define PARAMS_BLOB_SIZE   22u
#define PARAMS_GAIN_MAX    100000L
#define PARAMS_TRIM_MAX    1000

typedef struct {
    int32_t kp;
    int32_t ki;
    int32_t kd;
    int32_t ks;
    int16_t trim;
} params_t;

void    params_default(params_t *p);
uint8_t params_valid(const params_t *p);
void    params_serialize(const params_t *p, uint8_t blob[PARAMS_BLOB_SIZE]);
/* 반환 1: 성공, 0: magic, version, CRC, 범위 중 하나라도 실패 */
uint8_t params_deserialize(params_t *p, const uint8_t blob[PARAMS_BLOB_SIZE]);
uint8_t crc8(const uint8_t *data, uint8_t n);

#endif
