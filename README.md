# BalanceBot 임베디드 올인원 과정

C 언어 기초부터 ATmega328P 레지스터 펌웨어, KiCad 쉴드 PCB, Fusion 360 기구, Android 블루투스 앱, 통합 테스트, 포트폴리오까지 2륜 밸런싱 로봇 하나로 따라가는 자습 과정입니다.

## 시작하기

1. `part01_orientation/README.md` 를 읽고 부품을 주문합니다.
2. `site/index.html` (학습 사이트) 또는 각 파트의 `README.md` 를 순서대로 따라갑니다.
3. 모든 파트가 공유하는 핀맵, 프로토콜, 치수는 `SPEC.md` 에 있습니다.

## 폴더

| 폴더 | 내용 | 확인 방법 |
|---|---|---|
| `part02_c/` | C 실습 18개 | `make run` |
| `part03_avr/` | ATmega328P 실습 15개 | `make` (avr-gcc) |
| `part04_design/` | 요구사항, HSI, 추적성 | CSV |
| `part05_hw/` | 회로 설계 문서, BOM, 블록도 | |
| `part06_mech/` | STL, 도면, 생성 스크립트 | `python3 cad/generate.py` |
| `part07_app/` | Android 앱 소스 | Android Studio, `test/ProtocolTest.java` |
| `part08_fw/` | 밸런싱 펌웨어, 단위 테스트, 시뮬레이터 | `make`, `make test` |
| `part09_test/` | 통합 테스트 명세, 보고서 템플릿 | |
| `part10_portfolio/` | 포트폴리오 템플릿, 면접 질문 | |
| `tools/` | 학습 사이트 빌드 스크립트 | `python3 tools/build_site.py` |

`part08_fw/prebuilt/balancebot.hex` 는 기본 설정으로 미리 빌드한 펌웨어입니다. 장착 방향이 SPEC 과 다르면 `config.h` 를 고쳐 다시 빌드합니다.
