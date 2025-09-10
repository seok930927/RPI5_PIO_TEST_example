#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <string.h>
#include "../utils/piolib/include/piolib.h"
#include "../include/wizchip_qspi_pio.pio.h"
#include <gpiod.h>

// 핀 정의 (wizchip_qspi_pio.c와 호환)
#define QSPI_DATA_IO0_PIN   20
#define QSPI_DATA_IO1_PIN   21
#define QSPI_DATA_IO2_PIN   22
#define QSPI_DATA_IO3_PIN   23
#define QSPI_CLOCK_PIN      12
#define QSPI_CS_PIN         16

// QSPI 모드 정의 (wizchip_qspi_pio.h와 호환) 
#define QSPI_SINGLE_MODE    1
#define QSPI_DUAL_MODE      2
#define QSPI_QUAD_MODE      4

// 현재 모드 설정
#define _WIZCHIP_QSPI_MODE_ QSPI_QUAD_MODE


static volatile int keep_running = 1;

    // QSPI 테스트 루프
void signal_handler(int sig) {
    (void)sig;
    printf("\n시그널 받음, 종료 중...\n");
    keep_running = 0;
}

int main(int argc, char *argv[]) {
    PIO pio;
    int sm;
    pio_sm_config c;
    uint offset;
    int gpio_base = 7;  // QSPI 핀: GPIO 20부터 시작 (4비트: 20-23)  
    int gpio_cs = 16;    // CS 핀 (BCM 번호)
    int gpio_clk = 12;   // CLK 핀

    // gpiod 초기화 (모든 핀을 libgpiod로 제어)
    struct gpiod_chip *chip = gpiod_chip_open_by_number(0);
    if (!chip) {
        fprintf(stderr, "gpiod_chip_open_by_number 실패\n");
        return 1;
    }
    
    // CS 핀
    struct gpiod_line *cs_line = gpiod_chip_get_line(chip, gpio_cs);
    if (!cs_line || gpiod_line_request_output(cs_line, "qspi_cs", 1) < 0) {
        fprintf(stderr, "CS 핀 초기화 실패\n");
        gpiod_chip_close(chip);
        return 1;
    }
    
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);
    
    printf("=== QSPI Quad 테스트 + DMA (GPIO 20-23) ===\n");
    
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
    c = pio_get_default_sm_config();
    sm_config_set_wrap(&c, offset, offset + 8);  // wrap 설정 (상수로 고정)

    // Quad SPI를 위한 추가 설정
    sm_config_set_out_pins(&c, gpio_base, 4);    // 데이터 핀: GPIO 20-23
    sm_config_set_in_pins(&c, gpio_base);        // 입력 핀
    sm_config_set_set_pins(&c, gpio_base, 4);    // set pins도 데이터 핀으로 설정
    sm_config_set_clkdiv(&c, 5);               // 더 빠른 클럭으로 변경 (2.5MHz)

    sm_config_set_sideset(&c, 1, false, false);  // CLK를 sideset으로 사용
    sm_config_set_sideset_pins(&c, gpio_clk);    // CLK 핀 설정

    sm_config_set_in_shift(&c, true, true, 16);
    sm_config_set_out_shift(&c, true, true, 32 );

    // RP2350 스타일 PIO 설정 (QSPI Quad 모드)
    printf("\r\n[QSPI QUAD MODE]\r\n");
    
    // PIO 기능으로 GPIO 핀 설정
    for (int i = 0; i < 4; i++) {
        pio_gpio_init(pio, gpio_base + i);
    }
    pio_gpio_init(pio, gpio_clk);
    // 핀 방향 설정 (출력으로)
    pio_sm_set_consecutive_pindirs(pio, sm, gpio_base, 4, true);
    pio_sm_set_consecutive_pindirs(pio, sm, gpio_clk, 1, true);


    // 데이터 핀 풀다운 및 슈미트 트리거 활성화
    for (int i = 0; i < 4; i++) {
        gpio_set_pulls(gpio_base + i, true, true);
        gpio_set_input_enabled(gpio_base + i, true);
    }

    
    pio_sm_init(pio, sm, offset, &c);

    
     uint8_t test_patterns[80] =    {0x88, 0xff, 0xff, 0xff, 
                                    0x03, 0xFF, 0xff, 0xff,
                                    0xff, 0x56, 0x78,0x34, 
                                    0x56,0xff, 0x00, 0x00, 
                                    0xff, 0x12, 0x34, 0x56,
                                    0x78,0x34, 0x56, 0x78,
                                    0x34, 0x56, 0x78,0x34,
                                    0x34, 0x56, 0x78,0x34,
                                    0x88, 0xff, 0xff, 0xff, 
                                    0x03, 0xFF, 0xff, 0xff,
                                    0xff, 0x56, 0x78,0x34, 
                                    0x56,0xff, 0x00, 0x00, 
                                    0xff, 0x12, 0x34, 0x56,
                                    0x78,0x34, 0x56, 0x78,
                                    0x34, 0x56, 0x78,0x34,
                                    0x34, 0x56, 0x78,0x34,
                                    0x88, 0xff, 0xff, 0xff, 
                                    0x03, 0xFF, 0xff, 0xff,
                                    0xff, 0x56, 0x78,0x34, 
                                    0x56,0xff, 0x00, 0x00, 
                                    0xff, 0x12, 0x34, 0x56,
                                    0x78,0x34, 0x56, 0x78,
                                    0x34, 0x56, 0x78,0x34,
                                    0x34, 0x56, 0x78,0x34
                                    } ; // Quad read with data
     uint32_t test_patterns2[9] =    {0x88ffffff, 
                                    0x0202ffff,
                                    0xff567834, 
                                    0x56ff0000, 
                                    0x00123456,
                                    0x78345678,
                                    0x34ff5678,
                                    0x34ff5678,
                                    0x56010101} ; // Quad read with data
    size_t pattern_lengths[] = {4, 4, 4, 1, 4, 8};
    int pattern_count = sizeof(test_patterns) / sizeof(test_patterns[0]);
   
    // pio_sm_set_enabled(pio, sm, true);
    printf("FIFO 클리어 완료\n");

    printf("QSPI Quad 데이터 전송 시작 (GPIO 20-23, CLK 12, CS 16)...\n");
    printf("로직 애널라이저로 GPIO 20-23, CLK(12), CS(16) 확인하세요!\n");

    while (keep_running) {
        // CS low (칩 선택)
        gpiod_line_set_value(cs_line, 0);

        // SM 비활성화하고 재설정
        pio_sm_set_enabled(pio, sm, false);
        
        // FIFO 클리어
        pio_sm_clear_fifos(pio, sm);

        // PIO 재시작
        pio_sm_restart(pio, sm);
        pio_sm_clkdiv_restart(pio, sm);
        
        size_t send_size = 32;
        size_t total_bytes = send_size ; 
        size_t nibble_count = send_size * 2;       // 72니블 (Quad 모드)

// X레지스터 길이 지정 - X는  Max loop count        
#if 0
        //  pio_sm_exec(pio, sm, pio_encode_set(pio_x, ((nibble_count *4)- 1) ));  // 71
        //⚠️pio_encode_set() = 최대 31까지만 가능.
#else
        pio_sm_put_blocking(pio, sm, 0x00008FFF);               // TX FIFO <= 값
        pio_sm_exec(pio, sm, pio_encode_pull(false, true));     // OSR <= TX FIFO
        pio_sm_exec(pio, sm, pio_encode_mov(pio_x, pio_osr));   // X <= OSR
        pio_sm_exec(pio, sm, pio_encode_out(pio_null, 32));     // OSR의 32비트를 그냥 폐기
#endif 

        //SM 활성화
        pio_sm_set_enabled(pio, sm, true);        
        /*
            이때부터 클럭 생성시작.....
        */
        // FIFO 클리어
        pio_sm_clear_fifos(pio, sm);

        //DMA buffer 설정 
        pio_sm_config_xfer(pio, sm, PIO_DIR_TO_SM, 512, 1);  // 9개, 4바이트 단위

        pio_sm_exec(pio, sm, pio_encode_set(pio_y, 0));
        pio_sm_exec(pio, sm, pio_encode_pull(false, true));
        
        //Dma 버퍼에 데이터 전송 
        int sent = pio_sm_xfer_data(pio, sm, PIO_DIR_TO_SM, 80, test_patterns); 
        // pio_sm_xfer_data(pio, sm, PIO_DIR_TO_SM, 8, test_patterns+8); 
        // pio_sm_xfer_data(pio, sm, PIO_DIR_TO_SM, 8, test_patterns+8); 
     
        // 잠시 대기
        usleep(200);
        
        /*
        이때 부터 클럭 생성 종료.....
        */
        // SM 비활성화
        pio_sm_set_enabled(pio, sm, false);

        // CS high (칩 선택 해제)
        gpiod_line_set_value(cs_line, 1);

        // 다음 패턴으로 순환

        usleep(100);

    }
    
    printf("\n정리 중...\n");
    pio_sm_set_enabled(pio, sm, false);
    pio_remove_program(pio, &wizchip_pio_spi_quad_write_read_program, offset);
    pio_sm_unclaim(pio, sm);
    pio_close(pio);
    // gpiod 해제
    gpiod_line_release(cs_line);
    gpiod_chip_close(chip);
    printf("완료\n");
    return 0;

}
