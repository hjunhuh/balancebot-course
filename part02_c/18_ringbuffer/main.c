/*
 * main.c : 링버퍼 미니 프로젝트 (UART 수신 버퍼 흉내)
 *
 * [학습 목표]
 *  1. 여러 .c 파일을 함께 빌드하는 방법을 익힙니다.
 *       gcc -Wall -Wextra -std=c99 main.c ringbuf.c -o ringbuffer
 *     Dev-C++ 에서는 "파일 > 새로 만들기 > 프로젝트 > Console Application(C)" 로
 *     프로젝트를 만들고 세 파일을 프로젝트에 추가한 뒤 빌드합니다.
 *  2. 링버퍼의 넣기, 꺼내기, 순환(wrap around), 가득 참(overflow)을 출력으로 확인합니다.
 *  3. "ISR 이 바이트를 넣고, 메인 루프가 꺼내 한 줄 프레임을 조립" 하는
 *     실제 펌웨어 구조를 PC 에서 흉내 냅니다.
 */

#include <stdio.h>
#include <string.h>
#include "ringbuf.h"   /* 직접 만든 헤더는 < > 대신 " " 로 포함합니다 */

static RingBuf g_rx;   /* UART 수신 버퍼 (펌웨어에서는 ISR 과 공유하는 전역) */

static void show(const char *label)
{
    printf("%-26s head=%3u tail=%3u count=%u free=%u overflow=%u\n", label,
           (unsigned)g_rx.head, (unsigned)g_rx.tail, (unsigned)rb_count(&g_rx),
           (unsigned)rb_free(&g_rx), (unsigned)g_rx.overflow);
}

/* 실제 AVR 에서는 이렇게 생겼습니다.
 *   ISR(USART_RX_vect) { rb_put(&g_rx, UDR0); }
 * PC 에서는 이 함수를 직접 불러 "바이트가 도착했다" 고 흉내 냅니다. */
static void fake_uart_rx_isr(uint8_t byte)
{
    rb_put(&g_rx, byte);
}

/* 메인 루프에서 호출: 버퍼의 바이트를 꺼내 한 줄을 만듭니다.
   '\n' 을 만나면 1 을 돌려주고 line 에 완성된 문자열이 들어 있습니다. */
static int frame_poll(char *line, size_t size, size_t *len)
{
    uint8_t c;
    while (rb_get(&g_rx, &c)) {
        if (c == '\r') continue;               /* 프로토콜 v1: \r 무시 */
        if (c == '\n') {
            line[*len] = '\0';
            *len = 0;
            return 1;
        }
        if (*len + 1 < size) {
            line[(*len)++] = (char)c;
        } else {
            *len = 0;                          /* 너무 긴 프레임은 버림 (최대 48바이트) */
        }
    }
    return 0;
}

int main(void)
{
    printf("RB_SIZE=%u RB_MASK=0x%02X sizeof(RingBuf)=%u\n\n",
           (unsigned)RB_SIZE, (unsigned)RB_MASK, (unsigned)sizeof(RingBuf));

    printf("== 1. put and get ==\n");
    rb_init(&g_rx);
    show("init");
    for (uint8_t i = 1; i <= 5; i++) rb_put(&g_rx, (uint8_t)(i * 10));
    show("put 10,20,30,40,50");
    uint8_t v;
    printf("get:");
    for (int i = 0; i < 3; i++) {
        if (rb_get(&g_rx, &v)) printf(" %u", (unsigned)v);
    }
    printf("\n");
    show("after 3 gets");

    printf("\n== 2. wrap around ==\n");
    /* 끝(인덱스 7)을 넘어 앞쪽 칸을 다시 씁니다. FIFO 순서는 그대로 유지됩니다. */
    for (uint8_t i = 6; i <= 11; i++) rb_put(&g_rx, (uint8_t)(i * 10));
    show("put 60..110");
    printf("slot:");
    for (unsigned i = 0; i < RB_SIZE; i++) printf(" [%u]=%3u", i, (unsigned)g_rx.buf[i]);
    printf("\nget:");
    while (rb_get(&g_rx, &v)) printf(" %u", (unsigned)v);
    printf("\n");
    show("empty again");

    printf("\n== 3. overflow ==\n");
    for (unsigned i = 0; i < RB_SIZE + 3; i++) {
        uint8_t ok = rb_put(&g_rx, (uint8_t)('A' + i));
        printf("%c%s", 'A' + i, ok ? "" : "(drop)");
        printf(i + 1 < RB_SIZE + 3 ? " " : "\n");
    }
    show("after 11 puts");
    printf("get:");
    while (rb_get(&g_rx, &v)) printf(" %c", v);
    printf("\n");

    printf("\n== 4. head/tail counters roll over 255 -> 0 ==\n");
    rb_init(&g_rx);
    g_rx.head = g_rx.tail = 254;               /* 오래 돌아서 카운터가 끝에 온 상황 */
    for (uint8_t i = 0; i < 4; i++) rb_put(&g_rx, i);
    show("put 4 bytes from 254");             /* head 는 2 로 돌았지만 count 는 4 */
    printf("get:");
    while (rb_get(&g_rx, &v)) printf(" %u", (unsigned)v);
    printf("\n");

    printf("\n== 5. UART simulation: ISR puts, main loop builds frames ==\n");
    rb_init(&g_rx);
    /* 블루투스로 들어오는 바이트 흐름. 버퍼(8바이트)보다 길기 때문에
       ISR 이 몇 바이트 넣을 때마다 메인 루프가 꺼내 가야 넘치지 않습니다. */
    const char *stream = "$G*47\r\n$M,1*50\n$T,-35*53\n";
    size_t n = strlen(stream);
    char line[49];
    size_t len = 0;
    for (size_t i = 0; i < n; i++) {
        fake_uart_rx_isr((uint8_t)stream[i]);   /* 바이트 도착 (인터럽트) */
        if (i % 4 == 3 || i == n - 1) {         /* 메인 루프가 4바이트마다 한 번 돈다고 가정 */
            while (frame_poll(line, sizeof(line), &len)) {
                printf("frame: \"%s\"\n", line);
            }
        }
    }
    show("end of stream");

    printf("\n== 6. main loop too slow -> overflow ==\n");
    rb_init(&g_rx);
    len = 0;
    for (size_t i = 0; i < n; i++) {
        fake_uart_rx_isr((uint8_t)stream[i]);   /* 메인 루프가 한 번도 꺼내지 않음 */
    }
    show("no polling");
    while (frame_poll(line, sizeof(line), &len)) {
        printf("frame: \"%s\"\n", line);
    }
    printf("partial line left: %u bytes (frames lost)\n", (unsigned)len);

    return 0;
}

/*
 * [연습 문제]
 *  1. RB_SIZE 를 6 으로 바꾸어 빌드해 보세요 (gcc -DRB_SIZE=6u ...).
 *     어떤 메시지가 나오며, 왜 이 검사가 필요한지 설명해 보세요.
 *  2. 5번 시뮬레이션에서 메인 루프가 9바이트마다 한 번 돈다고 바꾸면
 *     (i % 9 == 8) 어떤 프레임이 깨지는지 확인하고, 안전한 RB_SIZE 를 정해 보세요.
 *     힌트: 9600 bps 는 초당 약 960바이트, 제어 주기는 5 ms 입니다.
 *  3. 버퍼를 비우지 않고 다음 바이트를 엿보는 uint8_t rb_peek(const RingBuf *rb,
 *     uint8_t *out) 함수를 ringbuf.h 와 ringbuf.c 에 추가해 보세요.
 */
