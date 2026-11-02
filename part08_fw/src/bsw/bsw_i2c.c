/*
 * bsw_i2c.c
 * SCL = F_CPU / (16 + 2 * TWBR * 4^TWPS) = 16e6 / (16 + 24) = 400 kHz (TWBR = 12, TWPS = 0)
 *
 * 상태 코드 (TWSR & 0xF8)
 *   0x08 START 전송, 0x10 반복 START 전송
 *   0x18 SLA+W 후 ACK, 0x28 데이터 송신 후 ACK
 *   0x40 SLA+R 후 ACK, 0x50 데이터 수신 후 ACK 보냄, 0x58 데이터 수신 후 NACK 보냄
 * 어느 단계든 기대 상태가 아니거나 시간이 초과되면 STOP 후 0 을 반환합니다.
 * (센서 선이 빠져도 무한 대기로 멈추지 않게 하는 것이 핵심)
 */
#include <avr/io.h>
#include "bsw_i2c.h"

#define I2C_TIMEOUT 3000u   /* 대기 루프 횟수 (약 1 ms 이상) */

static uint8_t wait_twint(void)
{
    uint16_t n = I2C_TIMEOUT;
    while (!(TWCR & (1 << TWINT))) {
        if (--n == 0u) return 0;
    }
    return 1;
}

static uint8_t status(void)
{
    return (uint8_t)(TWSR & 0xF8u);
}

static void stop(void)
{
    uint16_t n = I2C_TIMEOUT;
    TWCR = (1 << TWINT) | (1 << TWEN) | (1 << TWSTO);
    while (TWCR & (1 << TWSTO)) {
        if (--n == 0u) break;
    }
}

static uint8_t start(void)
{
    TWCR = (1 << TWINT) | (1 << TWSTA) | (1 << TWEN);
    if (!wait_twint()) return 0;
    return (status() == 0x08u || status() == 0x10u) ? 1u : 0u;
}

static uint8_t write_byte(uint8_t b, uint8_t expect)
{
    TWDR = b;
    TWCR = (1 << TWINT) | (1 << TWEN);
    if (!wait_twint()) return 0;
    return (status() == expect) ? 1u : 0u;
}

static uint8_t read_byte(uint8_t *b, uint8_t ack)
{
    TWCR = (uint8_t)((1 << TWINT) | (1 << TWEN) | (ack ? (1 << TWEA) : 0));
    if (!wait_twint()) return 0;
    if (status() != (ack ? 0x50u : 0x58u)) return 0;
    *b = TWDR;
    return 1;
}

static void bus_reset(void)
{
    TWCR = 0;               /* TWI 를 껐다 켜서 내부 상태 초기화 */
    bsw_i2c_init();
}

void bsw_i2c_init(void)
{
    TWSR = 0;               /* 분주 1 */
    TWBR = 12;
    TWCR = (1 << TWEN);
}

uint8_t bsw_i2c_write_reg(uint8_t addr7, uint8_t reg, uint8_t val)
{
    uint8_t ok = start()
              && write_byte((uint8_t)(addr7 << 1), 0x18u)
              && write_byte(reg, 0x28u)
              && write_byte(val, 0x28u);
    stop();
    if (!ok) bus_reset();
    return ok;
}

uint8_t bsw_i2c_read_regs(uint8_t addr7, uint8_t reg, uint8_t *buf, uint8_t n)
{
    uint8_t ok = start()
              && write_byte((uint8_t)(addr7 << 1), 0x18u)
              && write_byte(reg, 0x28u)
              && start()                                   /* 반복 START */
              && write_byte((uint8_t)((addr7 << 1) | 1u), 0x40u);
    for (uint8_t i = 0; ok && i < n; i++) {
        ok = read_byte(&buf[i], (uint8_t)(i + 1u < n));    /* 마지막 바이트만 NACK */
    }
    stop();
    if (!ok) bus_reset();
    return ok;
}
