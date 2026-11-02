/* bsw_tick.h : Timer1 CTC 1 ms 시스템 틱 */
#ifndef BSW_TICK_H
#define BSW_TICK_H
#include <stdint.h>

void     bsw_tick_init(void);
uint32_t bsw_tick_ms(void);

#endif
