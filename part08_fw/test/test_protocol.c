/* test_protocol.c : 프로토콜 파서, 생성기, 명령 처리 테스트 */
#include <string.h>
#include "test_util.h"
#include "protocol.h"
#include "cmd.h"
#include "params.h"

static proto_res_t feed_str(proto_rx_t *rx, const char *s, proto_msg_t *m)
{
    proto_res_t last = PROTO_RES_NONE;
    for (const char *p = s; *p; p++) {
        proto_res_t r = proto_rx_feed(rx, *p, m);
        if (r != PROTO_RES_NONE) last = r;
    }
    return last;
}

int main(void)
{
    proto_rx_t rx;
    proto_msg_t m;
    char out[64];

    /* SPEC 예시 체크섬 */
    CHECK(proto_checksum("G", 1) == 0x47);
    CHECK(proto_checksum("M,1", 3) == 0x50);

    proto_rx_init(&rx, '$');
    CHECK(feed_str(&rx, "$G*47\n", &m) == PROTO_RES_MSG);
    CHECK(m.type == 'G' && m.nfields == 0);

    CHECK(feed_str(&rx, "$M,1*50\r\n", &m) == PROTO_RES_MSG);
    CHECK(m.type == 'M' && m.nfields == 1 && m.f[0] == 1);

    /* 생성 후 다시 파싱 (왕복) */
    int32_t f[2] = { -35, 100 };
    uint8_t n = proto_build(out, sizeof(out), '$', 'D', f, 2);
    CHECK(n > 0 && out[n - 1] == '\n');
    printf("  built: %s", out);
    CHECK(feed_str(&rx, out, &m) == PROTO_RES_MSG);
    CHECK(m.type == 'D' && m.nfields == 2 && m.f[0] == -35 && m.f[1] == 100);

    /* 잘못된 체크섬, 형식 */
    CHECK(feed_str(&rx, "$M,1*51\n", &m) == PROTO_RES_ERR_CKSUM);
    CHECK(feed_str(&rx, "$M,*61\n", &m) == PROTO_RES_ERR_FORMAT);
    CHECK(feed_str(&rx, "$m,1*70\n", &m) == PROTO_RES_ERR_FORMAT);
    CHECK(feed_str(&rx, "$G47\n", &m) == PROTO_RES_ERR_FORMAT);
    /* 너무 긴 줄 */
    CHECK(feed_str(&rx, "$P,1111111111111111111111111111111111111111111111111111*00\n", &m) == PROTO_RES_ERR_FORMAT);
    /* 잡음 뒤 재동기화 */
    CHECK(feed_str(&rx, "xx$G*4", &m) == PROTO_RES_NONE);
    CHECK(feed_str(&rx, "$G*47\n", &m) == PROTO_RES_MSG);

    /* INT32 경계 생성 */
    int32_t big[1] = { -2147483647 - 1 };
    n = proto_build(out, sizeof(out), '#', 'E', big, 1);
    CHECK(n > 0 && strncmp(out, "#E,-2147483648*", 15) == 0);
    /* 버퍼 부족 */
    CHECK(proto_build(out, 8, '#', 'E', big, 1) == 0);

    /* ACK */
    n = proto_build_ack(out, sizeof(out), 'S');
    CHECK(n == 8 && strncmp(out, "#O,S*", 5) == 0);
    printf("  ack:   %s", out);

    /* 명령 처리 */
    params_t p;
    params_default(&p);
    cmd_ctx_t c;
    cmd_init(&c, &p);

    m.type = 'P'; m.nfields = 3; m.f[0] = 3000; m.f[1] = 150; m.f[2] = 90;
    n = cmd_handle(&c, &m, out, sizeof(out));
    CHECK(n > 0 && out[0] == '#' && out[1] == 'K');
    CHECK(p.kp == 3000 && p.ki == 150 && p.kd == 90 && c.params_changed == 1);
    printf("  #K:    %s", out);

    m.type = 'P'; m.f[0] = -1;
    n = cmd_handle(&c, &m, out, sizeof(out));
    CHECK(strncmp(out, "#E,3*", 5) == 0);
    CHECK(p.kp == 3000);                       /* 범위 오류면 값 유지 */

    m.type = 'D'; m.nfields = 2; m.f[0] = 50; m.f[1] = -20;
    CHECK(cmd_handle(&c, &m, out, sizeof(out)) == 0);
    CHECK(c.joy_x == 50 && c.joy_y == -20);
    for (int i = 0; i < 99; i++) cmd_tick(&c, 5);   /* 495 ms */
    CHECK(c.joy_x == 50);
    cmd_tick(&c, 5);                                /* 500 ms: 끊김 */
    CHECK(c.joy_x == 0 && c.joy_y == 0);

    m.type = 'S'; m.nfields = 0;
    CHECK(cmd_handle(&c, &m, out, sizeof(out)) == 0 && c.save_req == 1);

    m.type = 'Z'; m.nfields = 0;
    cmd_handle(&c, &m, out, sizeof(out));
    CHECK(strncmp(out, "#E,4*", 5) == 0);

    m.type = 'M'; m.nfields = 1; m.f[0] = 0;
    cmd_handle(&c, &m, out, sizeof(out));
    CHECK(c.motor_enable == 0 && strncmp(out, "#O,M*", 5) == 0);

    /* Part 9 TC-06 에 쓰는 프레임을 그대로 검증 */
    {
        const char *frames[4] = { "$G*47\n", "$G*48\n", "$Q*51\n", "$P,-1,0,0*60\n" };
        const char *expect[4] = { "#K,", "#E,1*", "#E,4*", "#E,3*" };
        for (int i = 0; i < 4; i++) {
            proto_res_t r = feed_str(&rx, frames[i], &m);
            if (r == PROTO_RES_MSG) n = cmd_handle(&c, &m, out, sizeof(out));
            else if (r == PROTO_RES_ERR_CKSUM) n = cmd_build_error(CMD_ERR_CKSUM, out, sizeof(out));
            else n = cmd_build_error(CMD_ERR_FORMAT, out, sizeof(out));
            CHECK(n > 0 && strncmp(out, expect[i], strlen(expect[i])) == 0);
        }
    }

    TEST_REPORT("protocol");
}
