/*
 * main.c
 * 밸런싱 로봇 펌웨어 진입점과 협력형 스케줄러.
 *
 *   [ASW]  balance (상태머신, 칼만, PID)   cmd (명령 해석)   protocol   params
 *     |  물리 단위 / 메시지
 *   [DRV]  mpu6050   motor   battery
 *     |  바이트 / 듀티 / ADC 값
 *   [BSW]  tick  gpio  pwm  uart  i2c  adc  eeprom
 *     |  레지스터
 *   [HW]   ATmega328P
 *
 * 작업 주기
 *   매 루프  : 워치독(250 ms) 리셋, UART 수신 처리, EEPROM 비차단 쓰기 진행
 *   5 ms     : IMU 읽기 -> balance_step -> 모터 출력
 *   100 ms   : 배터리, 버튼, LED, 텔레메트리 #A 송신
 */
#include <avr/io.h>
#include <avr/interrupt.h>
#include <avr/wdt.h>

#include "config.h"
#include "bsw_tick.h"
#include "bsw_gpio.h"
#include "bsw_pwm.h"
#include "bsw_uart.h"
#include "bsw_i2c.h"
#include "bsw_adc.h"
#include "bsw_eeprom.h"
#include "mpu6050.h"
#include "motor.h"
#include "battery.h"
#include "protocol.h"
#include "params.h"
#include "cmd.h"
#include "balance.h"

#define EEPROM_PARAMS_ADDR 0u
#define IMU_FAIL_LIMIT     3u

static params_t    g_params;
static cmd_ctx_t   g_cmd;
static balance_t   g_bal;
static proto_rx_t  g_rx;
static uint8_t     g_imu_ok;
static uint8_t     g_imu_fail;
static uint8_t     g_save_pending;    /* EEPROM 쓰기 진행 중, 끝나면 응답 */
static char        g_txbuf[64];

/* 가속도/자이로 축 선택: SPEC 장착 기준 (X 앞, Z 위, Y 왼쪽) */
static void imu_to_input(const mpu6050_raw_t *r, bal_input_t *in)
{
    in->ax_g   = (float)r->ax / 16384.0f;
    in->az_g   = (float)r->az / 16384.0f;
    in->gy_dps = (float)r->gy / 131.0f;
}

static void send(const char *s, uint8_t len)
{
    if (len > 0u) {
        (void)bsw_uart_write(s, len);
    }
}

static void load_params(void)
{
    uint8_t blob[PARAMS_BLOB_SIZE];
    bsw_eeprom_read(EEPROM_PARAMS_ADDR, blob, PARAMS_BLOB_SIZE);
    if (!params_deserialize(&g_params, blob)) {
        params_default(&g_params);           /* 처음 부팅 또는 손상: 기본값 */
    }
}

static void start_save(void)
{
    uint8_t blob[PARAMS_BLOB_SIZE];
    params_serialize(&g_params, blob);
    if (bsw_eeprom_write_async(EEPROM_PARAMS_ADDR, blob, PARAMS_BLOB_SIZE)) {
        g_save_pending = 1;
    }
}

static void handle_rx(void)
{
    char c;
    proto_msg_t m;
    uint8_t budget = 32;                     /* 한 루프에 너무 오래 머물지 않게 */
    while (budget-- && bsw_uart_getc(&c)) {
        proto_res_t r = proto_rx_feed(&g_rx, c, &m);
        uint8_t n = 0;
        if (r == PROTO_RES_MSG) {
            n = cmd_handle(&g_cmd, &m, g_txbuf, sizeof(g_txbuf));
        } else if (r == PROTO_RES_ERR_CKSUM) {
            n = cmd_build_error(CMD_ERR_CKSUM, g_txbuf, sizeof(g_txbuf));
        } else if (r == PROTO_RES_ERR_FORMAT) {
            n = cmd_build_error(CMD_ERR_FORMAT, g_txbuf, sizeof(g_txbuf));
        }
        send(g_txbuf, n);

        if (g_cmd.params_changed) {
            g_cmd.params_changed = 0;
            balance_set_params(&g_bal, &g_params);
        }
        if (g_cmd.save_req) {
            g_cmd.save_req = 0;
            if (!g_save_pending) start_save();
        }
    }
}

static void task_control(void)
{
    bal_input_t  in;
    bal_output_t out;
    mpu6050_raw_t raw;

    cmd_tick(&g_cmd, CTRL_PERIOD_MS);

    if (!g_imu_ok) {
        motor_set(0, 0);
        return;
    }
    if (!mpu6050_read(&raw)) {
        if (++g_imu_fail >= IMU_FAIL_LIMIT) {
            g_imu_ok = 0;                    /* 연속 실패: 모터 정지, 100 ms 작업에서 재초기화 */
            motor_set(0, 0);
        }
        return;                              /* 한두 번 실패는 이전 출력 유지 */
    }
    g_imu_fail = 0;

    imu_to_input(&raw, &in);
    in.vbat_mv = battery_mv();
    in.joy_x = g_cmd.joy_x;
    in.joy_y = g_cmd.joy_y;
    in.motor_enable = g_cmd.motor_enable;

    balance_step(&g_bal, &in, &out);
    motor_set(out.pwm_l, out.pwm_r);
}

static void update_led(uint8_t slow_count)
{
    if (!g_imu_ok) {                         /* IMU 오류: 100 ms 토글 */
        bsw_gpio_led_toggle();
        return;
    }
    switch (g_bal.st) {
    case BAL_BALANCE:      bsw_gpio_led(1); break;
    case BAL_CALIB:        bsw_gpio_led_toggle(); break;
    case BAL_WAIT_UPRIGHT:
    case BAL_FALLEN:       bsw_gpio_led((slow_count % 10u) < 5u); break;      /* 1 Hz */
    case BAL_LOW_BATT:     bsw_gpio_led((slow_count % 10u) == 0u || (slow_count % 10u) == 2u); break;
    default:               bsw_gpio_led(0); break;
    }
}

static void task_slow(void)
{
    static uint8_t count;
    static uint8_t btn_prev;
    count++;

    (void)battery_update_mv();

    /* 버튼: 100 ms 간격으로 읽으므로 그 자체가 디바운스 역할. 누를 때마다 모터 허용 토글 */
    uint8_t btn = bsw_gpio_button_pressed();
    if (btn && !btn_prev) {
        g_cmd.motor_enable = (uint8_t)!g_cmd.motor_enable;
    }
    btn_prev = btn;

    if (!g_imu_ok && (count % 10u) == 0u) {  /* 1 s 마다 IMU 재시도 */
        wdt_reset();                         /* 초기화에 약 150 ms 걸림 (워치독 250 ms 안) */
        g_imu_ok = mpu6050_init();
        g_imu_fail = 0;
        if (g_imu_ok) balance_init(&g_bal, &g_params);   /* 다시 보정부터 */
    }

    update_led(count);

    /* 텔레메트리 #A,angle,out,mv,st */
    int32_t f[4];
    f[0] = (int32_t)(g_bal.angle * 100.0f);
    f[1] = (int32_t)g_bal.out;
    f[2] = (int32_t)battery_mv();
    f[3] = g_imu_ok ? (int32_t)g_bal.st : (int32_t)BAL_INIT;
    send(g_txbuf, proto_build(g_txbuf, sizeof(g_txbuf), '#', 'A', f, 4));
}

int main(void)
{
    uint8_t reset_cause = MCUSR;
    MCUSR = 0;
    wdt_disable();
    (void)reset_cause;                       /* 디버깅 시 텔레메트리로 보내 볼 수 있음 */

    bsw_gpio_init();
    bsw_pwm_init();
    motor_init();
    bsw_tick_init();
    bsw_uart_init();
    bsw_i2c_init();
    bsw_adc_init();
    battery_init();

    load_params();
    cmd_init(&g_cmd, &g_params);
    proto_rx_init(&g_rx, '$');

    sei();

    g_imu_ok = mpu6050_init();
    balance_init(&g_bal, &g_params);

    wdt_enable(WDTO_250MS);                  /* 루프가 250 ms 이상 멈추면 리셋 */

    uint32_t t_ctrl = bsw_tick_ms();
    uint32_t t_slow = t_ctrl;

    for (;;) {
        wdt_reset();
        handle_rx();

        bsw_eeprom_poll();
        if (g_save_pending && !bsw_eeprom_busy()) {
            g_save_pending = 0;
            send(g_txbuf, proto_build_ack(g_txbuf, sizeof(g_txbuf), 'S'));
            send(g_txbuf, cmd_build_params(&g_params, g_txbuf, sizeof(g_txbuf)));
        }

        uint32_t now = bsw_tick_ms();
        if ((uint32_t)(now - t_ctrl) >= CTRL_PERIOD_MS) {
            t_ctrl += CTRL_PERIOD_MS;
            if ((uint32_t)(now - t_ctrl) >= 4u * CTRL_PERIOD_MS) {
                t_ctrl = now;                /* 크게 밀렸으면 따라잡지 않고 재동기화 */
            }
            task_control();
        }
        if ((uint32_t)(now - t_slow) >= SLOW_PERIOD_MS) {
            t_slow += SLOW_PERIOD_MS;
            task_slow();
        }
    }
}
