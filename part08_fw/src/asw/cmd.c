/*
 * cmd.c
 */
#include "cmd.h"
#include "config.h"

void cmd_init(cmd_ctx_t *c, params_t *p)
{
    c->params = p;
    c->joy_x = 0;
    c->joy_y = 0;
    c->joy_age_ms = DRIVE_TIMEOUT_MS;
    c->motor_enable = 1;
    c->save_req = 0;
    c->params_changed = 0;
}

uint8_t cmd_build_error(uint8_t code, char *reply, uint8_t reply_sz)
{
    int32_t f = code;
    return proto_build(reply, reply_sz, '#', 'E', &f, 1);
}

uint8_t cmd_build_params(const params_t *p, char *reply, uint8_t reply_sz)
{
    int32_t f[5];
    f[0] = p->kp;
    f[1] = p->ki;
    f[2] = p->kd;
    f[3] = p->trim;
    f[4] = p->ks;
    return proto_build(reply, reply_sz, '#', 'K', f, 5);
}

static uint8_t in_range(int32_t v, int32_t lo, int32_t hi)
{
    return (v >= lo && v <= hi) ? 1u : 0u;
}

uint8_t cmd_handle(cmd_ctx_t *c, const proto_msg_t *m, char *reply, uint8_t reply_sz)
{
    switch (m->type) {
    case 'D':
        if (m->nfields != 2u) return cmd_build_error(CMD_ERR_FORMAT, reply, reply_sz);
        if (!in_range(m->f[0], -100, 100) || !in_range(m->f[1], -100, 100)) {
            return cmd_build_error(CMD_ERR_RANGE, reply, reply_sz);
        }
        c->joy_x = (int8_t)m->f[0];
        c->joy_y = (int8_t)m->f[1];
        c->joy_age_ms = 0;
        return 0;                                  /* 50 ms 주기 명령이라 응답 생략 */

    case 'P':
        if (m->nfields != 3u) return cmd_build_error(CMD_ERR_FORMAT, reply, reply_sz);
        for (uint8_t i = 0; i < 3u; i++) {
            if (!in_range(m->f[i], 0, PARAMS_GAIN_MAX)) {
                return cmd_build_error(CMD_ERR_RANGE, reply, reply_sz);
            }
        }
        c->params->kp = m->f[0];
        c->params->ki = m->f[1];
        c->params->kd = m->f[2];
        c->params_changed = 1;
        return cmd_build_params(c->params, reply, reply_sz);

    case 'T':
        if (m->nfields != 1u) return cmd_build_error(CMD_ERR_FORMAT, reply, reply_sz);
        if (!in_range(m->f[0], -PARAMS_TRIM_MAX, PARAMS_TRIM_MAX)) {
            return cmd_build_error(CMD_ERR_RANGE, reply, reply_sz);
        }
        c->params->trim = (int16_t)m->f[0];
        c->params_changed = 1;
        return cmd_build_params(c->params, reply, reply_sz);

    case 'V':
        if (m->nfields != 1u) return cmd_build_error(CMD_ERR_FORMAT, reply, reply_sz);
        if (!in_range(m->f[0], 0, PARAMS_GAIN_MAX)) {
            return cmd_build_error(CMD_ERR_RANGE, reply, reply_sz);
        }
        c->params->ks = m->f[0];
        c->params_changed = 1;
        return cmd_build_params(c->params, reply, reply_sz);

    case 'M':
        if (m->nfields != 1u) return cmd_build_error(CMD_ERR_FORMAT, reply, reply_sz);
        if (!in_range(m->f[0], 0, 1)) return cmd_build_error(CMD_ERR_RANGE, reply, reply_sz);
        c->motor_enable = (uint8_t)m->f[0];
        return proto_build_ack(reply, reply_sz, 'M');

    case 'S':
        if (m->nfields != 0u) return cmd_build_error(CMD_ERR_FORMAT, reply, reply_sz);
        c->save_req = 1;                           /* 저장 완료 후 main 이 #O,S 와 #K 송신 */
        return 0;

    case 'G':
        if (m->nfields != 0u) return cmd_build_error(CMD_ERR_FORMAT, reply, reply_sz);
        return cmd_build_params(c->params, reply, reply_sz);

    default:
        return cmd_build_error(CMD_ERR_UNKNOWN, reply, reply_sz);
    }
}

void cmd_tick(cmd_ctx_t *c, uint16_t elapsed_ms)
{
    if (c->joy_age_ms < DRIVE_TIMEOUT_MS) {
        c->joy_age_ms += elapsed_ms;
        if (c->joy_age_ms >= DRIVE_TIMEOUT_MS) {
            c->joy_x = 0;                          /* 통신 끊김: 제자리 밸런싱 */
            c->joy_y = 0;
        }
    }
}
