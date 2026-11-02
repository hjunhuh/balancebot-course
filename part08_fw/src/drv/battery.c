/*
 * battery.c
 * V_adc = V_bat * 10k / (20k + 10k) = V_bat / 3
 * mV    = adc * 5000 / 1023 * 3 = adc * 15000 / 1023
 * 만충 8.4 V -> 2.8 V -> adc 573 정도
 */
#include "battery.h"
#include "config.h"
#include "bsw_adc.h"

#define AVG_N 8u

static uint16_t s_hist[AVG_N];
static uint8_t  s_idx;
static uint32_t s_sum;
static uint16_t s_mv;

static uint16_t sample_mv(void)
{
    uint32_t adc = bsw_adc_read(0);
    return (uint16_t)((adc * (uint32_t)VBAT_ADC_REF_MV * VBAT_DIV_RATIO) / 1023u);
}

void battery_init(void)
{
    uint16_t v = sample_mv();
    s_sum = 0;
    for (uint8_t i = 0; i < AVG_N; i++) {
        s_hist[i] = v;
        s_sum += v;
    }
    s_idx = 0;
    s_mv = v;
}

uint16_t battery_update_mv(void)
{
    uint16_t v = sample_mv();
    s_sum -= s_hist[s_idx];
    s_hist[s_idx] = v;
    s_sum += v;
    s_idx = (uint8_t)((s_idx + 1u) % AVG_N);
    s_mv = (uint16_t)(s_sum / AVG_N);
    return s_mv;
}

uint16_t battery_mv(void)
{
    return s_mv;
}
