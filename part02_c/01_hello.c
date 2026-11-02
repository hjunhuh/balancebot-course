/*
 * 01_hello.c : 개발환경과 컴파일 과정
 *
 * [학습 목표]
 *  1. C 프로그램의 기본 뼈대(#include, main, return)를 설명할 수 있습니다.
 *  2. 소스 파일이 실행 파일이 되기까지의 4단계(전처리, 컴파일, 어셈블, 링크)를
 *     구분하고, 각 단계의 결과물을 gcc 옵션으로 직접 만들어 볼 수 있습니다.
 *  3. PC용 컴파일러(gcc)와 AVR용 크로스 컴파일러(avr-gcc)의 차이를 이해합니다.
 *
 * [빌드와 실행]
 *  gcc -Wall -Wextra -std=c99 01_hello.c -o 01_hello
 *  ./01_hello            (Windows 에서는 01_hello.exe)
 */

/*
 * ---------------------------------------------------------------
 * 컴파일 과정 4단계
 * ---------------------------------------------------------------
 * (1) 전처리 (preprocess)   : gcc -E 01_hello.c -o 01_hello.i
 *     #include 로 지정한 헤더 파일 내용을 그대로 붙여 넣고, #define 매크로를
 *     치환하고, 주석을 지웁니다. 결과물(.i)은 여전히 C 소스입니다.
 *     01_hello.i 를 열어 보면 stdio.h 가 수백 줄로 펼쳐져 있습니다.
 *
 * (2) 컴파일 (compile)      : gcc -S 01_hello.i -o 01_hello.s
 *     C 문장을 그 CPU 의 어셈블리어로 번역합니다. 문법 오류와 대부분의
 *     경고가 이 단계에서 나옵니다. PC 라면 x86-64 어셈블리,
 *     avr-gcc 라면 AVR 어셈블리(ldi, out, sbi 같은 명령)가 나옵니다.
 *
 * (3) 어셈블 (assemble)     : gcc -c 01_hello.s -o 01_hello.o
 *     어셈블리를 기계어로 바꾼 오브젝트 파일(.o)을 만듭니다. 아직 printf 의
 *     실제 코드 위치는 비어 있습니다(미해결 심볼).
 *
 * (4) 링크 (link)           : gcc 01_hello.o -o 01_hello
 *     여러 .o 파일과 표준 라이브러리를 합쳐 미해결 심볼(printf 등)의 주소를
 *     채우고 실행 파일을 만듭니다. "undefined reference to ..." 오류는
 *     이 단계의 오류입니다.
 *
 * 보통은 gcc 01_hello.c -o 01_hello 한 줄로 4단계를 한 번에 수행합니다.
 * gcc -save-temps 01_hello.c 로 빌드하면 .i, .s, .o 를 모두 남겨 줍니다.
 *
 * AVR 에서는 같은 흐름이 avr-gcc -mmcu=atmega328p 로 진행되고, 마지막에
 * avr-objcopy 로 .elf 를 .hex 로 바꾼 뒤 프로그래머(AVRISP mkII)로 칩에
 * 기록합니다. PC 에서 만든 실행 파일은 PC CPU 용이므로 AVR 에서 돌지 않습니다.
 * 이렇게 다른 CPU 용 코드를 만드는 컴파일러를 크로스 컴파일러라고 합니다.
 * ---------------------------------------------------------------
 */

#include <stdio.h>   /* printf 의 선언(원형)이 들어 있는 표준 헤더입니다 */

/*
 * main 함수는 프로그램의 시작점입니다.
 * PC 에서는 운영체제가 main 을 호출하고, main 이 돌려준 값(종료 코드)을 받습니다.
 * 0 은 정상 종료를 뜻합니다.
 * 운영체제가 없는 AVR 에서는 main 이 끝나면 돌아갈 곳이 없으므로
 * main 안에 무한 루프(for (;;) 또는 while (1))를 두고 절대 끝나지 않게 만듭니다.
 */
int main(void)
{
    /* \n 은 줄바꿈 문자입니다. printf 는 형식 문자열을 화면에 출력합니다. */
    printf("Hello, Embedded World!\n");

    /* %d 는 정수를 10진수로, %s 는 문자열을 출력하는 변환 지정자입니다. */
    printf("This course has %d parts. Now: %s\n", 10, "Part 2 (C basics)");

    printf("Build stages: %s -> %s -> %s -> %s\n",
           "preprocess(.i)", "compile(.s)", "assemble(.o)", "link(exe)");

    /* __STDC_VERSION__ 은 컴파일러가 미리 정의해 주는 매크로입니다.
       -std=c99 로 빌드하면 199901L 이 됩니다. */
    printf("__STDC_VERSION__ = %ld\n", (long)__STDC_VERSION__);

    return 0;   /* 운영체제에 "정상 종료" 를 알립니다 */
}

/*
 * [연습 문제]
 *  1. gcc -save-temps 01_hello.c -o 01_hello 로 빌드한 뒤 생성된 .i 파일의
 *     줄 수를 확인해 보세요. 원래 소스보다 왜 훨씬 길어졌을까요?
 *  2. printf 를 prinft 로 일부러 잘못 적고 빌드해 보세요. 경고와 오류가
 *     각각 어느 단계(컴파일 또는 링크)에서 나오는지 구분해 보세요.
 *  3. 자기 이름과 오늘 날짜를 %s 와 %d 를 사용해 한 줄로 출력해 보세요.
 */
