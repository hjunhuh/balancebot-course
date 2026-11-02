/*
 * ringbuf.h : 링버퍼(원형 큐) 모듈의 공개 인터페이스
 *
 * [학습 목표]
 *  1. 헤더 파일(.h)에는 "무엇을 제공하는가"(타입, 함수 원형)만 두고,
 *     구현(.c)과 분리하는 다중 파일 구조를 익힙니다.
 *  2. 헤더 가드(#ifndef ... #define ... #endif)로 중복 포함을 막습니다.
 *  3. 2의 거듭제곱 크기 링버퍼의 설계(인덱스 마스킹)를 이해합니다.
 *
 * [사용처]
 *  Part 8 펌웨어에서 UART 수신 인터럽트(USART_RX_vect)가 받은 바이트를
 *  rb_put 으로 넣고, 메인 루프가 rb_get 으로 꺼내 한 줄 프레임을 조립합니다.
 *  ISR 은 head 만, 메인 루프는 tail 만 바꾸므로(생산자 하나, 소비자 하나)
 *  인덱스가 8비트라면 인터럽트를 막지 않고도 안전하게 동작합니다.
 */

#ifndef RINGBUF_H     /* 이 이름이 아직 정의되지 않았을 때만 아래 내용을 포함 */
#define RINGBUF_H     /* 두 번째로 include 되면 #ifndef 가 거짓이 되어 통째로 건너뜀 */

#include <stdint.h>

/* 버퍼 크기는 반드시 2의 거듭제곱(2, 4, 8, ... 128)이어야 합니다.
 * 그래야 (index % SIZE) 대신 (index & (SIZE - 1)) 로 빠르게 감쌀 수 있습니다.
 * AVR 에는 나눗셈 명령이 없어 % 는 수십 사이클이 걸리지만 & 는 1 사이클입니다.
 * 이 데모에서는 동작을 눈으로 보기 쉽게 작게 잡았습니다 (실제 UART 는 64 정도). */
#ifndef RB_SIZE
#define RB_SIZE 8u
#endif
#define RB_MASK (RB_SIZE - 1u)

/* 전처리 단계에서 크기가 2의 거듭제곱인지 검사합니다 (04장 연습 문제의 식). */
/* 최대 128 인 이유: head, tail 이 8비트라 개수 (head - tail) 도 8비트입니다.
   256 이면 "가득 참(256)" 이 0 으로 보여 "비어 있음" 과 구분되지 않습니다. */
#if (RB_SIZE == 0u) || ((RB_SIZE & (RB_SIZE - 1u)) != 0u) || (RB_SIZE > 128u)
#error "RB_SIZE must be a power of two between 1 and 128"
#endif

typedef struct {
    /* ISR 과 메인 루프가 함께 보는 값이므로 volatile 입니다 (16장 참고). */
    volatile uint8_t head;           /* 다음에 쓸 위치 (생산자, ISR 이 증가) */
    volatile uint8_t tail;           /* 다음에 읽을 위치 (소비자, 메인 루프가 증가) */
    volatile uint8_t overflow;       /* 가득 차서 버린 바이트 수 (최대 255 에서 멈춤) */
    uint8_t buf[RB_SIZE];
} RingBuf;

/* 버퍼를 빈 상태로 초기화합니다. */
void    rb_init(RingBuf *rb);

/* 바이트 하나를 넣습니다. 성공 1, 가득 차서 버렸으면 0. (ISR 에서 호출) */
uint8_t rb_put(RingBuf *rb, uint8_t byte);

/* 바이트 하나를 꺼내 *out 에 넣습니다. 성공 1, 비어 있으면 0. (메인 루프에서 호출) */
uint8_t rb_get(RingBuf *rb, uint8_t *out);

/* 들어 있는 바이트 수, 남은 공간 */
uint8_t rb_count(const RingBuf *rb);
uint8_t rb_free(const RingBuf *rb);

#endif /* RINGBUF_H */

/*
 * [연습 문제] (프로젝트 전체 문제는 main.c 하단에 있습니다)
 *  1. 헤더 가드 세 줄을 지우고, main.c 에서 #include "ringbuf.h" 를 두 번 쓰면
 *     어떤 오류가 나는지 확인해 보세요.
 *  2. RB_SIZE 를 256 으로 허용하면 rb_count 가 가득 찬 버퍼에서 얼마를 돌려주는지
 *     계산해 보세요. head, tail 을 uint16_t 로 바꾸면 최대 크기는 얼마까지 늘어날까요?
 */
