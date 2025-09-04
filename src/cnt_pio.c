#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include "../utils/piolib/include/piolib.h"

// 0x00~0x07 카운터를 위한 FIFO 기반 PIO 프로그램
// pioseq 스타일을 out pins로 변경
static const uint16_t counter_program_instructions[] = {
    // .wrap_target
    0x80a0,  //  0: pull   block           // TX FIFO에서 데이터 가져오기 → OSR
    0xa027,  //  1: mov    x, osr          // OSR → X 레지스터 (카운터 시작값)
    0xa0e1,  //  2: mov    osr, x          // X → OSR (출력 준비) – 0xA041 → 0xA0E1로 수정
    0x6003,  //  3: out    pins, 3         // OSR의 3비트를 GPIO에 출력
    0x0042,  //  4: jmp    x--, 2          // X 감소 후 2번으로 점프 (루프)
};

static const pio_program_t counter_program = {
    .instructions = counter_program_instructions,
    .length = 5,
    .origin = -1,
};

static volatile int keep_running = 1;

void signal_handler(int sig) {
    printf("\n시그널 받음, 종료 중...\n");
    keep_running = 0;
}

int main(int argc, char *argv[]) {
    PIO pio;
    int sm;
    pio_sm_config c;
    uint offset;
    int gpio_base = 13;  // GPIO 13부터 시작 (3비트: 13-15)
    
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);
    
    printf("=== 0x00~0x07 카운터 테스트 (GPIO 13-15) ===\n");
    
    // PIO 초기화
    pio = pio_open(0);
    if (pio == NULL) {
        fprintf(stderr, "PIO 열기 실패\n");
        return 1;
    }
    
    sm = pio_claim_unused_sm(pio, true);
    if (sm < 0) {
        fprintf(stderr, "SM 할당 실패\n");
        pio_close(pio);
        return 1;
    }
    
    offset = pio_add_program(pio, &counter_program);
    printf("프로그램이 오프셋 %d에 로드됨, SM %d 사용\n", offset, sm);
    
    // SM 설정
    c = pio_get_default_sm_config();
    sm_config_set_wrap(&c, offset + 0, offset + 4);           // wrap 설정
    sm_config_set_out_pins(&c, gpio_base, 3);                // GPIO 13-15, 3핀
    sm_config_set_clkdiv(&c, 1);                          // 클럭을 느리게 (관찰 가능하게)
    
    // GPIO 13-15을 PIO가 제어하도록 설정
    for (int i = 0; i < 3; i++) {
        pio_gpio_init(pio, gpio_base + i);
        pio_sm_set_consecutive_pindirs(pio, sm, gpio_base + i, 1, true);  // 출력으로 설정
    }
    
    // SM 초기화 및 시작
    pio_sm_init(pio, sm, offset, &c);
    
    // FIFO 클리어
    pio_sm_clear_fifos(pio, sm);
    printf("FIFO 클리어 완료\n");
    
    // SM 활성화
    pio_sm_set_enabled(pio, sm, true);
    printf("SM 활성화 완료\n");
    
    printf("카운터 시작 (0x00-0x07 반복, GPIO 13-15)...\n");
    printf("로직 애널라이저로 GPIO 13-15 확인하세요!\n");
    
    // 카운터 루프
    uint32_t start_value = 255;
    int counter = 0;
    while (keep_running) {
        // 255부터 시작해서 카운트다운 (0xFF → 0x00)
    //    uint32_t start_value = 255;
        
        // FIFO에 시작값 전송
        pio_sm_put_blocking(pio, sm, 7);

        // pio_sm_put_blocking(pio, sm, 5);
        // pio_sm_put_blocking(pio, sm, 6);
        // pio_sm_put_blocking(pio, sm, 7);
        // pio_sm_put_blocking(pio, sm, 0);
        // pio_sm_put_blocking(pio, sm, 0);
        // sleep(0.01);  // 1초 대기 후 다음 사이클
        // sleep(0.1);  // 1초 대기 후 다음 사이클



    }
    
    printf("\n정리 중...\n");
    pio_sm_set_enabled(pio, sm, false);
    pio_remove_program(pio, &counter_program, offset);
    pio_sm_unclaim(pio, sm);
    pio_close(pio);
    
    printf("완료\n");
    return 0;
}
