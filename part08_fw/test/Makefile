# PC 단위 테스트와 시뮬레이터 (gcc)
CC     = gcc
CFLAGS = -std=c99 -Wall -Wextra -O2 -I../src -I../src/asw -I../src/drv -I.
HDRS   = $(wildcard ../src/*.h ../src/asw/*.h ../src/drv/*.h) test_util.h
ASW    = ../src/asw/protocol.c ../src/asw/cmd.c ../src/asw/params.c \
         ../src/asw/pid.c ../src/asw/kalman.c ../src/asw/balance.c
BINS   = test_protocol test_params test_pid_kalman sim_balance

all: $(BINS)

test_protocol: test_protocol.c $(ASW) $(HDRS)
	$(CC) $(CFLAGS) $(filter %.c,$^) -o $@ -lm
test_params: test_params.c $(ASW) $(HDRS)
	$(CC) $(CFLAGS) $(filter %.c,$^) -o $@ -lm
test_pid_kalman: test_pid_kalman.c $(ASW) $(HDRS)
	$(CC) $(CFLAGS) $(filter %.c,$^) -o $@ -lm
# motor.c 의 데드존 매핑을 쓰기 위해 BSW 를 흉내 내는 stub 을 함께 링크
sim_balance: sim_balance.c stub_bsw.c ../src/drv/motor.c $(ASW) $(HDRS)
	$(CC) $(CFLAGS) -I../src/bsw $(filter %.c,$^) -o $@ -lm

run: all
	./test_protocol && ./test_params && ./test_pid_kalman && ./sim_balance

clean:
	rm -f $(BINS)

.PHONY: all run clean
