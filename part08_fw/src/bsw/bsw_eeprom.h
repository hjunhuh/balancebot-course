/* bsw_eeprom.h : 레지스터 수준 EEPROM 읽기, 비차단(한 바이트씩) 쓰기 */
#ifndef BSW_EEPROM_H
#define BSW_EEPROM_H
#include <stdint.h>

#define BSW_EEPROM_ASYNC_MAX 32u

void    bsw_eeprom_read(uint16_t addr, uint8_t *buf, uint8_t n);   /* 차단, 빠름 */
uint8_t bsw_eeprom_write_async(uint16_t addr, const uint8_t *buf, uint8_t n); /* 1 접수 */
void    bsw_eeprom_poll(void);                                     /* 메인 루프에서 계속 호출 */
uint8_t bsw_eeprom_busy(void);

#endif
