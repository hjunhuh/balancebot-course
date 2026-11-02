/*
 * params.c
 */
#include "params.h"
#include "config.h"

uint8_t crc8(const uint8_t *data, uint8_t n)
{
    uint8_t crc = 0x00;
    for (uint8_t i = 0; i < n; i++) {
        crc ^= data[i];
        for (uint8_t b = 0; b < 8u; b++) {
            if (crc & 0x80u) {
                crc = (uint8_t)((crc << 1) ^ 0x07u);
            } else {
                crc = (uint8_t)(crc << 1);
            }
        }
    }
    return crc;
}

void params_default(params_t *p)
{
    p->kp = DEFAULT_KP;
    p->ki = DEFAULT_KI;
    p->kd = DEFAULT_KD;
    p->ks = DEFAULT_KS;
    p->trim = DEFAULT_TRIM;
}

uint8_t params_valid(const params_t *p)
{
    if (p->kp < 0 || p->kp > PARAMS_GAIN_MAX) return 0;
    if (p->ki < 0 || p->ki > PARAMS_GAIN_MAX) return 0;
    if (p->kd < 0 || p->kd > PARAMS_GAIN_MAX) return 0;
    if (p->ks < 0 || p->ks > PARAMS_GAIN_MAX) return 0;
    if (p->trim < -PARAMS_TRIM_MAX || p->trim > PARAMS_TRIM_MAX) return 0;
    return 1;
}

static void put32(uint8_t *b, int32_t v)
{
    uint32_t u = (uint32_t)v;
    b[0] = (uint8_t)(u);
    b[1] = (uint8_t)(u >> 8);
    b[2] = (uint8_t)(u >> 16);
    b[3] = (uint8_t)(u >> 24);
}

static int32_t get32(const uint8_t *b)
{
    uint32_t u = (uint32_t)b[0]
               | ((uint32_t)b[1] << 8)
               | ((uint32_t)b[2] << 16)
               | ((uint32_t)b[3] << 24);
    return (int32_t)u;
}

void params_serialize(const params_t *p, uint8_t blob[PARAMS_BLOB_SIZE])
{
    blob[0] = (uint8_t)(PARAMS_MAGIC & 0xFFu);
    blob[1] = (uint8_t)(PARAMS_MAGIC >> 8);
    blob[2] = PARAMS_VERSION;
    put32(&blob[3], p->kp);
    put32(&blob[7], p->ki);
    put32(&blob[11], p->kd);
    put32(&blob[15], p->ks);
    blob[19] = (uint8_t)((uint16_t)p->trim & 0xFFu);
    blob[20] = (uint8_t)((uint16_t)p->trim >> 8);
    blob[21] = crc8(blob, 21u);
}

uint8_t params_deserialize(params_t *p, const uint8_t blob[PARAMS_BLOB_SIZE])
{
    uint16_t magic = (uint16_t)(blob[0] | ((uint16_t)blob[1] << 8));
    if (magic != PARAMS_MAGIC) return 0;
    if (blob[2] != PARAMS_VERSION) return 0;
    if (crc8(blob, 21u) != blob[21]) return 0;

    params_t t;
    t.kp = get32(&blob[3]);
    t.ki = get32(&blob[7]);
    t.kd = get32(&blob[11]);
    t.ks = get32(&blob[15]);
    t.trim = (int16_t)(uint16_t)(blob[19] | ((uint16_t)blob[20] << 8));
    if (!params_valid(&t)) return 0;

    *p = t;
    return 1;
}
