#!/bin/sh
# 칼만 설정별로 시뮬레이터를 다시 빌드해, 게인 108 조합 x 무게중심 높이 3 가지 = 324 경우 중
# 몇 개가 통과하는지 셉니다.  사용: cd test && sh sweep_kalman.sh
for cfg in "0.003 0.03" "0.003 3.0" "0.0001 3.0" "0.00001 30.0"; do
  set -- $cfg
  gcc -std=c99 -O2 -I../src -I../src/asw -I../src/drv -I../src/bsw \
      -DKF_Q_BIAS=${1}f -DKF_R_MEASURE=${2}f \
      sim_balance.c stub_bsw.c ../src/drv/motor.c ../src/asw/*.c -o sim_sweep -lm
  n=0
  for L in 0.07 0.10 0.13; do
  for kp in 10 15 20 30; do for ki in 50 100 200; do for kd in 0.25 0.5 1.0; do for ks in 5 10 20; do
    SIM_L=$L ./sim_sweep $kp $ki $kd $ks | tail -1 | grep -q PASS && n=$((n+1))
  done; done; done; done; done
  echo "Q_BIAS=$1 R=$2 : 통과 $n / 324"
done
rm -f sim_sweep
