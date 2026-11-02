# Part 3 ATmega328P 실습 일괄 빌드
#   make          : 모든 lab 을 빌드하여 각 lab 폴더에 main.hex 생성, 크기 출력
#   make lab05_timer_tick : 한 lab 만 빌드
#   make clean    : 빌드 산출물 삭제
#
# 각 lab 은 main.c 하나로 단독 빌드됩니다. Microchip Studio 에서는 main.c 를 새 프로젝트에 넣고
# Symbols 에 F_CPU=16000000UL 을 추가하면 같은 결과를 얻습니다.

MCU     = atmega328p
F_CPU   = 16000000UL
CC      = avr-gcc
OBJCOPY = avr-objcopy
SIZE    = avr-size

CFLAGS  = -mmcu=$(MCU) -DF_CPU=$(F_CPU) -Os -Wall -Wextra -std=gnu99 \
          -ffunction-sections -fdata-sections
LDFLAGS = -mmcu=$(MCU) -Wl,--gc-sections

LABS = $(sort $(dir $(wildcard lab*/main.c)))
LABS := $(LABS:/=)
HEXS = $(addsuffix /main.hex,$(LABS))

.PHONY: all clean size $(LABS)

all: $(HEXS) size

$(LABS): %: %/main.hex
	@$(SIZE) -C --mcu=$(MCU) $@/main.elf | grep -E 'Program|Data'

%/main.elf: %/main.c
	$(CC) $(CFLAGS) $< $(LDFLAGS) -o $@

%/main.hex: %/main.elf
	$(OBJCOPY) -O ihex -R .eeprom $< $@

size: $(HEXS)
	@echo ""
	@echo "==== 크기 요약 (text+data = Flash, data+bss = SRAM 정적 사용량) ===="
	@$(SIZE) -B $(addsuffix /main.elf,$(LABS))

clean:
	rm -f $(addsuffix /main.elf,$(LABS)) $(HEXS)

.SECONDARY:
