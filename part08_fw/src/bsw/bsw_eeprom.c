/*
 * bsw_eeprom.c
 *
 * EEPROM 한 바이트 쓰기는 약 3.3 ms 걸립니다. 22 바이트를 한 번에 쓰면 70 ms 이상
 * 제어 루프가 멈춰 로봇이 넘어지므로, 버퍼에 담아 두고 EEPE 가 0 일 때마다
 * 한 바이트씩 쓰는 비차단 방식으로 구현합니다.
 *
 * 쓰기 절차 (데이터시트 순서):
 *   1. EEPE 가 0 이 될 때까지 대기 (여기서는 0 일 때만 진입)
 *   2. EEAR 에 주소, EEDR 에 데이터
 *   3. EEMPE = 1, 그 뒤 4 클럭 안에 EEPE = 1 (그래서 인터럽트를 잠시 막음)
 * 수명 보호: 기존 값과 같으면 쓰지 않습니다 (셀당 약 10만 회).
 */
#include <avr/io.h>
#include <avr/interrupt.h>
#include "bsw_eeprom.h"

static uint8_t  s_buf[BSW_EEPROM_ASYNC_MAX];
static uint16_t s_addr;
static uint8_t  s_len;
static uint8_t  s_pos;

static uint8_t read_byte(uint16_t addr)
{
    while (EECR & (1 << EEPE)) { }
    EEAR = addr;
    EECR |= (1 << EERE);
    return EEDR;
}

void bsw_eeprom_read(uint16_t addr, uint8_t *buf, uint8_t n)
{
    for (uint8_t i = 0; i < n; i++) {
        buf[i] = read_byte((uint16_t)(addr + i));
    }
}

uint8_t bsw_eeprom_write_async(uint16_t addr, const uint8_t *buf, uint8_t n)
{
    if (s_pos < s_len || n > BSW_EEPROM_ASYNC_MAX) {
        return 0;                           /* 이전 쓰기 진행 중 또는 너무 큼 */
    }
    for (uint8_t i = 0; i < n; i++) {
        s_buf[i] = buf[i];
    }
    s_addr = addr;
    s_pos = 0;
    s_len = n;
    return 1;
}

void bsw_eeprom_poll(void)
{
    while (s_pos < s_len) {
        if (EECR & (1 << EEPE)) {
            return;                         /* 이전 바이트 쓰는 중: 다음 호출에서 계속 */
        }
        uint16_t a = (uint16_t)(s_addr + s_pos);
        uint8_t  d = s_buf[s_pos];
        s_pos++;
        if (read_byte(a) == d) {
            continue;                       /* 같은 값: 건너뛰고 다음 바이트 */
        }
        uint8_t sreg = SREG;
        cli();
        EEAR = a;
        EEDR = d;
        EECR |= (1 << EEMPE);
        EECR |= (1 << EEPE);
        SREG = sreg;
        return;                             /* 한 번에 한 바이트만 시작 */
    }
}

uint8_t bsw_eeprom_busy(void)
{
    return (s_pos < s_len || (EECR & (1 << EEPE))) ? 1u : 0u;
}
