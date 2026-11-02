/* battery.h : 배터리 전압 측정 (ADC0, 20k/10k 분압) */
#ifndef BATTERY_H
#define BATTERY_H
#include <stdint.h>

void     battery_init(void);
uint16_t battery_update_mv(void);   /* 100 ms 마다 호출, 8 샘플 이동평균 mV */
uint16_t battery_mv(void);

#endif
