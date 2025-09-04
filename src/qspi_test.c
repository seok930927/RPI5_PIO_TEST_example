#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include "../utils/piolib/include/piolib.h"
#include "wizchip_qspi_pio.pio"  // PIO 어셈블리어 레벨 그대로 사용

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
    int gpio_base = 0;  // QSPI 핀: GPIO 0부터 시작 (4비트: 0-3)
    int gpio_cs = 4;    // CS 핀
    int gpio_clk = 5;   // CLK 핀
    
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);
    
    printf("=== QSPI Quad 테스트 (GPIO 0-3) - PIO 어셈블리어 레벨 그대로 ===\n");
    
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
    
    // wizchip_qspi_pio.pio.h의 QSPI Quad 프로그램 로드
    offset = pio_add_program(pio, &wizchip_pio_spi_quad_write_read_program);
    printf("QSPI Quad 프로그램이 오프셋 %d에 로드됨, SM %d 사용\n", offset, sm);
    
    // 기본 설정 가져오기
    c = wizchip_pio_spi_quad_write_read_program_get_default_config(offset);
    
    // GPIO 설정
    sm_config_set_out_pins(&c, gpio_base, 4);  // 데이터 핀: GPIO 0-3
    sm_config_set_in_pins(&c, gpio_base);      // 입력 핀
    sm_config_set_set_pins(&c, gpio_cs, 1);    // CS 핀
    sm_config_set_sideset_pins(&c, gpio_clk);  // CLK 핀 (side-set)
    sm_config_set_clkdiv(&c, 125.0f);          // ~1MHz SPI 클럭
    
    // GPIO 초기화
    for (int i = 0; i < 4; i++) {
        pio_gpio_init(pio, gpio_base + i);
        pio_sm_set_consecutive_pindirs(pio, sm, gpio_base + i, 1, true);  // 출력
    }
    pio_gpio_init(pio, gpio_cs);
    pio_sm_set_consecutive_pindirs(pio, sm, gpio_cs, 1, true);  // CS 출력
    pio_gpio_init(pio, gpio_clk);
    pio_sm_set_consecutive_pindirs(pio, sm, gpio_clk, 1, true);  // CLK 출력
    
    // SM 초기화 및 시작
    pio_sm_init(pio, sm, offset, &c);
    
    // FIFO 클리어
    pio_sm_clear_fifos(pio, sm);
    printf("FIFO 클리어 완료\n");
    
    // SM 활성화
    pio_sm_set_enabled(pio, sm, true);
    printf("SM 활성화 완료\n");
    
    printf("QSPI Quad 데이터 전송 시작 (GPIO 0-3)...\n");
    printf("로직 애널라이저로 GPIO 0-3, CS(4), CLK(5) 확인하세요!\n");
    
    // QSPI 테스트 루프
    while (keep_running) {
        // 쓰기 모드: FIFO에 데이터 넣기
        pio_sm_put_blocking(pio, sm, 0x0F);  // 4비트 데이터 전송
        sleep(0.1);
        
        // 읽기 모드: FIFO에서 데이터 가져오기 (옵션)
        if (pio_sm_get_rx_fifo_level(pio, sm) > 0) {
            uint32_t rx_data = pio_sm_get_blocking(pio, sm);
            printf("RX 데이터: 0x%X\n", rx_data);
        }
    }
    
    printf("\n정리 중...\n");
    pio_sm_set_enabled(pio, sm, false);
    pio_remove_program(pio, &wizchip_pio_spi_quad_write_read_program, offset);
    pio_sm_unclaim(pio, sm);
    pio_close(pio);
    
    printf("완료\n");
    return 0;
}
