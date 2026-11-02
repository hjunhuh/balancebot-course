/* bsw_adc.h : 10bit ADC, AVcc 기준, 단일 변환 */
#ifndef BSW_ADC_H
#define BSW_ADC_H
#include <stdint.h>

void     bsw_adc_init(void);
uint16_t bsw_adc_read(uint8_t ch);    /* 0..1023 */

#endif
