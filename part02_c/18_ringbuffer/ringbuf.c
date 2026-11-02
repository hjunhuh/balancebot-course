/*
 * ringbuf.c : 링버퍼 구현
 *
 * [학습 목표]
 *  1. 헤더에 선언한 함수를 .c 에서 정의하고, 따로 컴파일해 링크하는 흐름을 익힙니다.
 *  2. head, tail 을 "자유롭게 증가하는 8비트 카운터" 로 두고 읽고 쓸 때만
 *     마스크로 위치를 구하는 방식을 이해합니다.
 *
 * [설계]
 *  head 와 tail 은 0..255 를 돌며 계속 증가합니다 (uint8_t 의 순환, 02장).
 *    들어 있는 개수 = (uint8_t)(head - tail)   : 넘쳐서 돌아도 뺄셈 결과는 올바릅니다
 *    실제 위치      = index & RB_MASK
 *  이렇게 하면 "가득 참" 과 "비어 있음" 을 구분하려고 한 칸을 비워 둘 필요가 없어
 *  RB_SIZE 칸을 모두 쓸 수 있습니다. 단, 이 방법은 RB_SIZE 가 256 을 나누어 떨어지게
 *  하는 크기(즉 2의 거듭제곱)일 때만 성립합니다.
 */

#include "ringbuf.h"   /* 자기 헤더를 먼저 포함하면 선언과 정의가 다를 때 바로 오류가 납니다 */

void rb_init(RingBuf *rb)
{
    rb->head = 0;
    rb->tail = 0;
    rb->overflow = 0;
}

uint8_t rb_count(const RingBuf *rb)
{
    return (uint8_t)(rb->head - rb->tail);
}

uint8_t rb_free(const RingBuf *rb)
{
    return (uint8_t)(RB_SIZE - rb_count(rb));
}

uint8_t rb_put(RingBuf *rb, uint8_t byte)
{
    uint8_t head = rb->head;                 /* volatile 은 한 번만 읽어 지역 변수로 */
    if ((uint8_t)(head - rb->tail) >= RB_SIZE) {
        if (rb->overflow < 255u) {
            rb->overflow++;
        }
        return 0;                            /* 가득 참: 새 바이트를 버립니다 */
    }
    rb->buf[head & RB_MASK] = byte;          /* 먼저 데이터를 쓰고 */
    rb->head = (uint8_t)(head + 1u);         /* 그다음 head 를 공개합니다 (순서 중요) */
    return 1;
}

uint8_t rb_get(RingBuf *rb, uint8_t *out)
{
    uint8_t tail = rb->tail;
    if (rb->head == tail) {
        return 0;                            /* 비어 있음 */
    }
    *out = rb->buf[tail & RB_MASK];          /* 먼저 데이터를 읽고 */
    rb->tail = (uint8_t)(tail + 1u);         /* 그다음 자리를 돌려줍니다 */
    return 1;
}

/*
 * [연습 문제] (프로젝트 전체 문제는 main.c 하단에 있습니다)
 *  1. rb_put 에서 데이터 쓰기와 head 증가의 순서를 바꾸면, ISR 과 메인 루프가
 *     동시에 동작할 때 어떤 문제가 생길 수 있는지 설명해 보세요.
 *  2. 가득 찼을 때 새 바이트를 버리는 대신 가장 오래된 바이트를 덮어쓰는
 *     rb_put_overwrite 를 작성해 보세요. 이 방식은 왜 ISR 과 메인 루프가 함께 쓰는
 *     UART 수신 버퍼에는 위험할까요? (힌트: 생산자가 tail 까지 바꾸게 됩니다)
 */
