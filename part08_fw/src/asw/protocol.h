/*
 * protocol.h
 * 블루투스 프로토콜 v1 (SPEC.md 4장) 프레임 조립, 파싱, 생성.
 *   $<TYPE>[,<int>...]*<HH>\n   (앱 -> 로봇)
 *   #<TYPE>[,<int>...]*<HH>\n   (로봇 -> 앱)
 * HH 는 시작 문자 다음부터 '*' 직전까지 XOR.
 */
#ifndef PROTOCOL_H
#define PROTOCOL_H

#include <stdint.h>

#define PROTO_MAX_LEN     48u
#define PROTO_MAX_FIELDS  6u

typedef struct {
    char    type;                        /* 'D', 'P', ... */
    uint8_t nfields;
    int32_t f[PROTO_MAX_FIELDS];
} proto_msg_t;

typedef enum {
    PROTO_RES_NONE = 0,       /* 아직 줄이 끝나지 않음 */
    PROTO_RES_MSG,            /* 정상 메시지 1개 완성 */
    PROTO_RES_ERR_CKSUM,      /* 체크섬 불일치 */
    PROTO_RES_ERR_FORMAT      /* 형식 오류, 길이 초과 */
} proto_res_t;

/* 바이트 단위 수신 조립기 */
typedef struct {
    char    start;                       /* 받을 프레임 시작 문자 */
    char    buf[PROTO_MAX_LEN + 1];
    uint8_t len;
    uint8_t overflow;
    uint8_t active;                      /* 시작 문자를 받은 뒤인지 */
} proto_rx_t;

void        proto_rx_init(proto_rx_t *rx, char start);
proto_res_t proto_rx_feed(proto_rx_t *rx, char c, proto_msg_t *out);

/* 한 줄('\n' 제외)을 해석 */
proto_res_t proto_parse_line(const char *line, uint8_t len, char start, proto_msg_t *out);

/* 프레임 생성. 반환값: '\n' 포함 길이, 버퍼 부족 시 0. 문자열 끝에 '\0' 을 둡니다. */
uint8_t proto_build(char *out, uint8_t out_sz, char start, char type,
                    const int32_t *fields, uint8_t nfields);
/* "#O,<c>" 처리 완료 프레임 */
uint8_t proto_build_ack(char *out, uint8_t out_sz, char cmd);

uint8_t proto_checksum(const char *s, uint8_t n);

#endif
