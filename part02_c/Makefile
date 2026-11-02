# Part 2 C 실습 Makefile
#   make        : 모든 실습을 build/ 폴더에 빌드합니다
#   make run    : 모든 실습을 차례로 실행합니다
#   make clean  : build/ 폴더를 지웁니다
#   make build/04_bitwise && ./build/04_bitwise : 하나만 빌드하고 실행
# 경고 0개가 기준입니다. 경고를 오류로 바꿔 확인하려면 make WERROR=1

CC      := gcc
CFLAGS  := -Wall -Wextra -std=c99
ifeq ($(WERROR),1)
CFLAGS  += -Werror
endif
BUILD   := build

SRCS    := $(sort $(wildcard [0-1][0-9]_*.c))
PROGS   := $(patsubst %.c,$(BUILD)/%,$(SRCS)) $(BUILD)/18_ringbuffer

RB_DIR  := 18_ringbuffer
RB_SRCS := $(RB_DIR)/main.c $(RB_DIR)/ringbuf.c

.PHONY: all run clean

all: $(PROGS)

$(BUILD):
	mkdir -p $(BUILD)

$(BUILD)/%: %.c | $(BUILD)
	$(CC) $(CFLAGS) $< -o $@

# 다중 파일 프로젝트: 헤더가 바뀌어도 다시 빌드되도록 의존성에 넣습니다
$(BUILD)/18_ringbuffer: $(RB_SRCS) $(RB_DIR)/ringbuf.h | $(BUILD)
	$(CC) $(CFLAGS) $(RB_SRCS) -o $@

run: all
	@for p in $(PROGS); do \
		echo "========== $$p =========="; \
		./$$p || exit 1; \
		echo; \
	done

clean:
	rm -rf $(BUILD)
