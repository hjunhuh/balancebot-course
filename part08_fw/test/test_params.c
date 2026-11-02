/* test_params.c : EEPROM 직렬화와 CRC 테스트 */
#include "test_util.h"
#include "params.h"

int main(void)
{
    params_t a, b;
    uint8_t blob[PARAMS_BLOB_SIZE];

    /* CRC-8 (다항식 0x07, 초기값 0) 표준 검증값: "123456789" -> 0xF4 */
    CHECK(crc8((const uint8_t *)"123456789", 9) == 0xF4);

    params_default(&a);
    a.kp = 3125; a.ki = 12000; a.kd = 75; a.ks = 250; a.trim = -137;
    params_serialize(&a, blob);
    CHECK(params_deserialize(&b, blob) == 1);
    CHECK(b.kp == 3125 && b.ki == 12000 && b.kd == 75 && b.ks == 250 && b.trim == -137);

    /* 한 비트 손상 -> 거부 */
    blob[5] ^= 0x01;
    CHECK(params_deserialize(&b, blob) == 0);
    blob[5] ^= 0x01;

    /* 빈 EEPROM (0xFF) -> 거부 */
    for (unsigned i = 0; i < PARAMS_BLOB_SIZE; i++) blob[i] = 0xFF;
    CHECK(params_deserialize(&b, blob) == 0);

    /* 범위 밖 값은 CRC 가 맞아도 거부 */
    a.trim = 2000;
    params_serialize(&a, blob);
    CHECK(params_deserialize(&b, blob) == 0);

    TEST_REPORT("params");
}
