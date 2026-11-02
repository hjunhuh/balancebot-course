/*
 * protocol.c
 * printf/sscanf 를 쓰지 않고 직접 파싱합니다 (AVR 코드 크기 절약, 동작 예측 가능).
 */
#include "protocol.h"

static const char HEX[] = "0123456789ABCDEF";

uint8_t proto_checksum(const char *s, uint8_t n)
{
    uint8_t x = 0;
    for (uint8_t i = 0; i < n; i++) {
        x ^= (uint8_t)s[i];
    }
    return x;
}

static int hex_val(char c)
{
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    return -1;
}

void proto_rx_init(proto_rx_t *rx, char start)
{
    rx->start = start;
    rx->len = 0;
    rx->overflow = 0;
    rx->active = 0;
}

proto_res_t proto_rx_feed(proto_rx_t *rx, char c, proto_msg_t *out)
{
    if (c == '\r') {
        return PROTO_RES_NONE;
    }
    if (c == rx->start) {
        /* 시작 문자는 항상 새 프레임의 시작으로 보고 다시 동기화 */
        rx->buf[0] = c;
        rx->len = 1;
        rx->overflow = 0;
        rx->active = 1;
        return PROTO_RES_NONE;
    }
    if (!rx->active) {
        return PROTO_RES_NONE;           /* 프레임 밖의 잡음은 버림 */
    }
    if (c == '\n') {
        proto_res_t r;
        if (rx->overflow) {
            r = PROTO_RES_ERR_FORMAT;
        } else {
            r = proto_parse_line(rx->buf, rx->len, rx->start, out);
        }
        rx->len = 0;
        rx->overflow = 0;
        rx->active = 0;
        return r;
    }
    if (rx->len < PROTO_MAX_LEN) {
        rx->buf[rx->len++] = c;
    } else {
        rx->overflow = 1;
    }
    return PROTO_RES_NONE;
}

proto_res_t proto_parse_line(const char *line, uint8_t len, char start, proto_msg_t *out)
{
    /* 가장 짧은 프레임: "$G*47" = 5 바이트 */
    if (len < 5u || line[0] != start) {
        return PROTO_RES_ERR_FORMAT;
    }
    uint8_t star = (uint8_t)(len - 3u);
    if (line[star] != '*') {
        return PROTO_RES_ERR_FORMAT;
    }
    int h = hex_val(line[star + 1u]);
    int l = hex_val(line[star + 2u]);
    if (h < 0 || l < 0) {
        return PROTO_RES_ERR_FORMAT;
    }
    if (proto_checksum(&line[1], (uint8_t)(star - 1u)) != (uint8_t)((h << 4) | l)) {
        return PROTO_RES_ERR_CKSUM;
    }

    char type = line[1];
    if (type < 'A' || type > 'Z') {
        return PROTO_RES_ERR_FORMAT;
    }
    out->type = type;
    out->nfields = 0;

    uint8_t i = 2;
    while (i < star) {
        if (line[i] != ',') {
            return PROTO_RES_ERR_FORMAT;
        }
        i++;
        if (out->nfields >= PROTO_MAX_FIELDS) {
            return PROTO_RES_ERR_FORMAT;
        }
        uint8_t neg = 0;
        if (i < star && line[i] == '-') {
            neg = 1;
            i++;
        }
        uint8_t digits = 0;
        int32_t v = 0;
        while (i < star && line[i] >= '0' && line[i] <= '9') {
            if (digits >= 9u) {
                return PROTO_RES_ERR_FORMAT;   /* 9자리까지만 허용 (int32 안전) */
            }
            v = v * 10 + (line[i] - '0');
            digits++;
            i++;
        }
        if (digits == 0u) {
            return PROTO_RES_ERR_FORMAT;       /* 빈 필드 또는 숫자가 아닌 문자 */
        }
        out->f[out->nfields++] = neg ? -v : v;
    }
    return PROTO_RES_MSG;
}

/* 정수를 10진 문자열로 붙임. 반환: 새 위치, 공간 부족 시 0xFF */
static uint8_t put_int(char *out, uint8_t pos, uint8_t cap, int32_t v)
{
    char tmp[11];
    uint8_t n = 0;
    uint32_t u;
    if (v < 0) {
        if (pos >= cap) return 0xFF;
        out[pos++] = '-';
        u = (uint32_t)(-(v + 1)) + 1u;     /* INT32_MIN 도 안전하게 */
    } else {
        u = (uint32_t)v;
    }
    do {
        tmp[n++] = (char)('0' + (u % 10u));
        u /= 10u;
    } while (u != 0u);
    while (n > 0u) {
        if (pos >= cap) return 0xFF;
        out[pos++] = tmp[--n];
    }
    return pos;
}

static uint8_t finish(char *out, uint8_t pos, uint8_t out_sz)
{
    /* "*HH\n\0" 에 5 바이트 필요 */
    if ((uint16_t)pos + 5u > out_sz) {
        return 0;
    }
    uint8_t cs = proto_checksum(&out[1], (uint8_t)(pos - 1u));
    out[pos++] = '*';
    out[pos++] = HEX[cs >> 4];
    out[pos++] = HEX[cs & 0x0F];
    out[pos++] = '\n';
    out[pos] = '\0';
    return pos;
}

uint8_t proto_build(char *out, uint8_t out_sz, char start, char type,
                    const int32_t *fields, uint8_t nfields)
{
    if (out_sz < 7u) {
        return 0;
    }
    uint8_t cap = (uint8_t)(out_sz - 5u);   /* 체크섬 자리 남김 */
    uint8_t pos = 0;
    out[pos++] = start;
    out[pos++] = type;
    for (uint8_t i = 0; i < nfields; i++) {
        if (pos >= cap) return 0;
        out[pos++] = ',';
        pos = put_int(out, pos, cap, fields[i]);
        if (pos == 0xFF) return 0;
    }
    return finish(out, pos, out_sz);
}

uint8_t proto_build_ack(char *out, uint8_t out_sz, char cmd)
{
    if (out_sz < 9u) {
        return 0;
    }
    uint8_t pos = 0;
    out[pos++] = '#';
    out[pos++] = 'O';
    out[pos++] = ',';
    out[pos++] = cmd;
    return finish(out, pos, out_sz);
}
