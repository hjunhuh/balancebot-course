/*
 * cmd.h
 * 수신 메시지 해석 (명령 처리). UART, EEPROM 을 직접 만지지 않고
 * "무엇을 해야 하는지"만 컨텍스트에 기록합니다. 실제 저장과 송신은 main 이 합니다.
 */
#ifndef CMD_H
#define CMD_H

#include <stdint.h>
#include "protocol.h"
#include "params.h"

#define CMD_ERR_CKSUM    1
#define CMD_ERR_FORMAT   2
#define CMD_ERR_RANGE    3
#define CMD_ERR_UNKNOWN  4

typedef struct {
    params_t *params;
    int8_t    joy_x;
    int8_t    joy_y;
    uint16_t  joy_age_ms;       /* 마지막 $D 이후 경과 시간 */
    uint8_t   motor_enable;
    uint8_t   save_req;         /* $S 수신: main 이 EEPROM 저장 시작 */
    uint8_t   params_changed;   /* 제어기에 새 게인 반영 필요 */
} cmd_ctx_t;

void    cmd_init(cmd_ctx_t *c, params_t *p);
/* 응답 프레임을 reply 에 쓰고 길이를 반환 (응답 없으면 0) */
uint8_t cmd_handle(cmd_ctx_t *c, const proto_msg_t *m, char *reply, uint8_t reply_sz);
uint8_t cmd_build_error(uint8_t code, char *reply, uint8_t reply_sz);
uint8_t cmd_build_params(const params_t *p, char *reply, uint8_t reply_sz);
/* 제어 주기마다 호출: 조이스틱 명령 끊김 감시 */
void    cmd_tick(cmd_ctx_t *c, uint16_t elapsed_ms);

#endif
