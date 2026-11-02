/*
 * lab11_i2c_mpu6050: TWI(I2C) 마스터 폴링 드라이버와 MPU6050 원시값 읽기
 *
 * 배선 (GY-521 모듈):
 *   VCC -> 5V (모듈에 3.3 V 레귤레이터 내장),  GND -> GND
 *   SCL -> A5 (PC5),  SDA -> A4 (PC4)
 *   AD0 -> 연결 안 함 또는 GND (주소 0x68),  INT -> D2 (예비, 이 실습에서는 사용 안 함)
 *   GY-521 에는 SDA/SCL 풀업 저항이 이미 달려 있으므로 AVR 내부 풀업은 켜지 않습니다.
 *   결과 출력: UART 9600 bps (USB 시리얼 모니터)
 *
 * TWI 클럭 (데이터시트 "2-wire Serial Interface", Bit Rate Generator Unit):
 *   SCL = F_CPU / (16 + 2 x TWBR x 분주) = 16000000 / (16 + 2 x 12 x 1) = 400 kHz
 *   TWSR 의 TWPS1:0 = 00 이면 분주 1
 *
 * 드라이버 동작 원리:
 *   TWCR 에 TWINT=1 을 써서 "다음 동작 시작" 을 지시하고, 하드웨어가 동작을 끝내면 TWINT 가 1 이 됩니다.
 *   그 뒤 TWSR 의 상위 5비트(상태 코드)를 확인합니다. 상태 코드 이름은 <util/twi.h> 에 있습니다.
 *   배선이 빠졌거나 센서가 SCL 을 붙잡고 있으면 TWINT 가 영원히 안 올라오므로 반드시 타임아웃을 둡니다.
 *
 * MPU6050 데이터는 레지스터 0x3B 부터 14바이트 (가속도 XYZ, 온도, 자이로 XYZ),
 * 각 값은 상위 바이트가 먼저 오는 빅엔디언 16비트 2의 보수입니다.
 */
#include <avr/io.h>
#include <avr/pgmspace.h>
#include <util/delay.h>
#include <util/twi.h>
#include <stdio.h>

/* ================= UART (lab09 와 동일) ================= */
static void uart_init(void)
{
    UBRR0  = 207;
    UCSR0A = (1 << U2X0);
    UCSR0B = (1 << TXEN0);
    UCSR0C = (1 << UCSZ01) | (1 << UCSZ00);
}
static int uart_stream_put(char c, FILE *stream)
{
    (void)stream;
    if (c == '\n') uart_stream_put('\r', stream);
    while (!(UCSR0A & (1 << UDRE0))) { }
    UDR0 = (uint8_t)c;
    return 0;
}
static FILE uart_out = FDEV_SETUP_STREAM(uart_stream_put, NULL, _FDEV_SETUP_WRITE);

/* ================= TWI 마스터 (폴링) ================= */
#define TWI_TIMEOUT_US  1000u     /* 400 kHz 에서 1바이트는 약 23 us, 넉넉히 1 ms */

typedef enum {
    TWI_OK = 0,
    TWI_ERR_TIMEOUT,              /* TWINT 가 시간 안에 안 올라옴 (배선, 버스 잠김) */
    TWI_ERR_START,                /* START 생성 실패 */
    TWI_ERR_NACK_ADDR,            /* 주소에 응답하는 장치 없음 */
    TWI_ERR_NACK_DATA,            /* 데이터 바이트를 거절함 */
    TWI_ERR_STATUS                /* 예상하지 못한 상태 코드 */
} twi_err_t;

static void twi_init(void)
{
    TWSR = 0;                                     /* TWPS1:0 = 00, 분주 1 */
    TWBR = 12;                                    /* 400 kHz */
    TWCR = (1 << TWEN);
}

/* TWINT 가 1 이 될 때까지 대기 (타임아웃 포함) */
static twi_err_t twi_wait(void)
{
    uint16_t t = TWI_TIMEOUT_US;
    while (!(TWCR & (1 << TWINT))) {
        if (t-- == 0) return TWI_ERR_TIMEOUT;
        _delay_us(1);
    }
    return TWI_OK;
}

/* 타임아웃 뒤 TWI 를 껐다 켜서 하드웨어 상태를 초기화 */
static void twi_reset(void)
{
    TWCR = 0;
    twi_init();
}

/* START (또는 반복 START) 후 SLA+R/W 전송. addr 는 7비트 주소, rw 는 TW_READ 또는 TW_WRITE */
static twi_err_t twi_start(uint8_t addr, uint8_t rw)
{
    TWCR = (1 << TWINT) | (1 << TWSTA) | (1 << TWEN);
    if (twi_wait()) return TWI_ERR_TIMEOUT;
    uint8_t st = TW_STATUS;                       /* TWSR & 0xF8 */
    if (st != TW_START && st != TW_REP_START) return TWI_ERR_START;

    TWDR = (uint8_t)((addr << 1) | rw);
    TWCR = (1 << TWINT) | (1 << TWEN);            /* TWSTA 를 지우지 않으면 START 를 또 보냅니다 */
    if (twi_wait()) return TWI_ERR_TIMEOUT;
    st = TW_STATUS;
    if (st == TW_MT_SLA_ACK || st == TW_MR_SLA_ACK) return TWI_OK;
    if (st == TW_MT_SLA_NACK || st == TW_MR_SLA_NACK) return TWI_ERR_NACK_ADDR;
    return TWI_ERR_STATUS;
}

static twi_err_t twi_write(uint8_t data)
{
    TWDR = data;
    TWCR = (1 << TWINT) | (1 << TWEN);
    if (twi_wait()) return TWI_ERR_TIMEOUT;
    uint8_t st = TW_STATUS;
    if (st == TW_MT_DATA_ACK) return TWI_OK;
    if (st == TW_MT_DATA_NACK) return TWI_ERR_NACK_DATA;
    return TWI_ERR_STATUS;
}

/* 1바이트 수신. ack=1 이면 "더 보내라"(ACK), 마지막 바이트는 ack=0 (NACK) */
static twi_err_t twi_read(uint8_t ack, uint8_t *data)
{
    TWCR = (uint8_t)((1 << TWINT) | (1 << TWEN) | (ack ? (1 << TWEA) : 0));
    if (twi_wait()) return TWI_ERR_TIMEOUT;
    uint8_t st = TW_STATUS;
    if ((ack && st != TW_MR_DATA_ACK) || (!ack && st != TW_MR_DATA_NACK)) return TWI_ERR_STATUS;
    *data = TWDR;
    return TWI_OK;
}

static void twi_stop(void)
{
    TWCR = (1 << TWINT) | (1 << TWSTO) | (1 << TWEN);
    uint16_t t = TWI_TIMEOUT_US;
    while (TWCR & (1 << TWSTO)) {                 /* STOP 이 버스에 나가면 TWSTO 가 자동으로 0 */
        if (t-- == 0) { twi_reset(); return; }
        _delay_us(1);
    }
}

/* ================= 레지스터 접근 도우미 ================= */
static twi_err_t i2c_write_reg(uint8_t dev, uint8_t reg, uint8_t val)
{
    twi_err_t e = twi_start(dev, TW_WRITE);
    if (!e) e = twi_write(reg);
    if (!e) e = twi_write(val);
    if (e == TWI_ERR_TIMEOUT) twi_reset();
    else twi_stop();
    return e;
}

/* reg 부터 n 바이트 연속 읽기: START, SLA+W, reg, 반복 START, SLA+R, 데이터..., STOP */
static twi_err_t i2c_read_regs(uint8_t dev, uint8_t reg, uint8_t *buf, uint8_t n)
{
    twi_err_t e = twi_start(dev, TW_WRITE);
    if (!e) e = twi_write(reg);
    if (!e) e = twi_start(dev, TW_READ);          /* STOP 없이 반복 START */
    for (uint8_t i = 0; !e && i < n; i++) {
        e = twi_read((uint8_t)(i < n - 1), &buf[i]);
    }
    if (e == TWI_ERR_TIMEOUT) twi_reset();
    else twi_stop();
    return e;
}

/* ================= MPU6050 ================= */
#define MPU_ADDR          0x68
#define MPU_SMPLRT_DIV    0x19
#define MPU_CONFIG        0x1A
#define MPU_GYRO_CONFIG   0x1B
#define MPU_ACCEL_CONFIG  0x1C
#define MPU_ACCEL_XOUT_H  0x3B
#define MPU_PWR_MGMT_1    0x6B
#define MPU_WHO_AM_I      0x75

typedef struct {
    int16_t ax, ay, az;     /* 16384 LSB/g  (+-2 g) */
    int16_t temp;           /* 온도 원시값 */
    int16_t gx, gy, gz;     /* 131 LSB/dps  (+-250 dps) */
} mpu_raw_t;

/* 빅엔디언 2바이트 결합: 상위 바이트를 먼저 8비트 올리고 하위 바이트를 OR */
static int16_t be16(const uint8_t *p)
{
    return (int16_t)(((uint16_t)p[0] << 8) | p[1]);
}

static twi_err_t mpu_init(void)
{
    twi_err_t e;
    e = i2c_write_reg(MPU_ADDR, MPU_PWR_MGMT_1, 0x01);   /* 슬립 해제, 클럭 = 자이로 X PLL */
    if (e) return e;
    _delay_ms(10);
    e = i2c_write_reg(MPU_ADDR, MPU_SMPLRT_DIV, 4);      /* 1 kHz / (1 + 4) = 200 Hz */
    if (!e) e = i2c_write_reg(MPU_ADDR, MPU_CONFIG, 3);  /* DLPF: 가속도 44 Hz, 자이로 42 Hz */
    if (!e) e = i2c_write_reg(MPU_ADDR, MPU_GYRO_CONFIG, 0x00);   /* +-250 dps */
    if (!e) e = i2c_write_reg(MPU_ADDR, MPU_ACCEL_CONFIG, 0x00);  /* +-2 g */
    return e;
}

static twi_err_t mpu_read_raw(mpu_raw_t *m)
{
    uint8_t b[14];
    twi_err_t e = i2c_read_regs(MPU_ADDR, MPU_ACCEL_XOUT_H, b, sizeof(b));
    if (e) return e;
    m->ax   = be16(&b[0]);
    m->ay   = be16(&b[2]);
    m->az   = be16(&b[4]);
    m->temp = be16(&b[6]);
    m->gx   = be16(&b[8]);
    m->gy   = be16(&b[10]);
    m->gz   = be16(&b[12]);
    return TWI_OK;
}

int main(void)
{
    DDRB |= (1 << PB2);
    uart_init();
    stdout = &uart_out;
    twi_init();
    _delay_ms(100);                                       /* 센서 전원 안정화 */

    printf_P(PSTR("\nlab11 MPU6050\n"));

    uint8_t who = 0;
    twi_err_t e = i2c_read_regs(MPU_ADDR, MPU_WHO_AM_I, &who, 1);
    printf_P(PSTR("WHO_AM_I err=%u val=0x%02X (expect 0x68)\n"), e, who);
    if (e || who != 0x68) {
        printf_P(PSTR("MPU6050 not found. check wiring, address(AD0)\n"));
        for (;;) {                                        /* 실패 표시: 빠른 깜박임 */
            PINB = (1 << PB2);
            _delay_ms(100);
        }
    }

    e = mpu_init();
    printf_P(PSTR("init err=%u\n"), e);

    mpu_raw_t m;
    for (;;) {
        e = mpu_read_raw(&m);
        if (e) {
            printf_P(PSTR("read err=%u\n"), e);
        } else {
            /* 온도: T[C] = raw / 340 + 36.53 -> x100 정수 */
            int16_t t100 = (int16_t)((int32_t)m.temp * 100 / 340 + 3653);
            printf_P(PSTR("A %6d %6d %6d  G %6d %6d %6d  T %d\n"),
                     m.ax, m.ay, m.az, m.gx, m.gy, m.gz, t100);
        }
        PINB = (1 << PB2);
        _delay_ms(200);
    }
}
