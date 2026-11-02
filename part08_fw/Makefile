# 밸런싱 로봇 펌웨어 빌드 (avr-gcc)
#   make        : 펌웨어 빌드 -> build/balancebot.hex
#   make test   : PC 에서 ASW 단위 테스트 + 시뮬레이션 실행
#   make flash  : avrdude + AVRISP mkII 로 기록 (Microchip Studio 를 쓰면 필요 없음)
MCU    = atmega328p
F_CPU  = 16000000UL
CC     = avr-gcc
OBJCOPY= avr-objcopy
SIZE   = avr-size

SRCDIRS = src src/bsw src/drv src/asw
SRCS    = $(foreach d,$(SRCDIRS),$(wildcard $(d)/*.c))
INC     = $(addprefix -I,$(SRCDIRS))
CFLAGS  = -mmcu=$(MCU) -DF_CPU=$(F_CPU) -Os -std=gnu99 -Wall -Wextra \
          -ffunction-sections -fdata-sections $(INC)
LDFLAGS = -mmcu=$(MCU) -Wl,--gc-sections -lm

all: build/balancebot.hex

build/balancebot.elf: $(SRCS) $(wildcard src/*.h src/*/*.h)
	@mkdir -p build
	$(CC) $(CFLAGS) $(SRCS) -o $@ $(LDFLAGS)

build/balancebot.hex: build/balancebot.elf
	$(OBJCOPY) -O ihex -R .eeprom $< $@
	$(SIZE) -C --mcu=$(MCU) $<

flash: build/balancebot.hex
	avrdude -c avrispmkII -P usb -p m328p -U flash:w:$<:i

test:
	$(MAKE) -C test run

clean:
	rm -rf build
	$(MAKE) -C test clean

.PHONY: all flash test clean
