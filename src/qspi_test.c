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

// QSPI 구성 구조체 (원본 wizchip_qspi_pio.h 기반)
typedef struct wiznet_spi_config {
    uint8_t data_io0_pin;
    uint8_t data_io1_pin; 
    uint8_t data_io2_pin;
    uint8_t data_io3_pin;
    uint8_t cs_pin;
    uint8_t clock_pin;
    uint8_t irq_pin;
    uint16_t clock_div_major;
    uint8_t clock_div_minor;
} wiznet_spi_config_t;

// QSPI 테스트 상태 구조체 (원본 wizchip_qspi_pio.c의 spi_pio_state 기반)
typedef struct qspi_test_state {
    const wiznet_spi_config_t *spi_config;
    PIO pio;
    int8_t pio_offset;
    int8_t pio_sm;
    struct gpiod_chip *chip;
    struct gpiod_line *cs_line;
    struct gpiod_line *data_lines[4];
} qspi_test_state_t;

static volatile int keep_running = 1;

void signal_handler(int sig) {
    (void)sig;
    printf("\n시그널 받음, 종료 중...\n");
    keep_running = 0;
}

// QSPI 커맨드 버퍼 생성 함수 (원본 wizchip_qspi_pio.c의 mk_cmd_buf 기반)
static uint16_t mk_qspi_cmd_buf(uint8_t *pdst, uint8_t opcode, uint16_t addr) {
#if (_WIZCHIP_QSPI_MODE_ == QSPI_QUAD_MODE)
    // QSPI Quad 모드: 각 비트를 4개 라인으로 분산
    pdst[0] = ((opcode >> 7 & 0x01) << 4) | ((opcode >> 6 & 0x01) << 0);
    pdst[1] = ((opcode >> 5 & 0x01) << 4) | ((opcode >> 4 & 0x01) << 0);
    pdst[2] = ((opcode >> 3 & 0x01) << 4) | ((opcode >> 2 & 0x01) << 0);
    pdst[3] = ((opcode >> 1 & 0x01) << 4) | ((opcode >> 0 & 0x01) << 0);
    
    pdst[4] = ((uint8_t)(addr >> 8) & 0xFF);
    pdst[5] = ((uint8_t)(addr >> 0) & 0xFF);
    pdst[6] = 0;  // 더미 바이트
    
    return 7;  // 7바이트 반환
#else
    // Single/Dual 모드는 나중에 구현
    pdst[0] = opcode;
    pdst[1] = (uint8_t)((addr >> 8) & 0xFF);
    pdst[2] = (uint8_t)((addr >> 0) & 0xFF);
    pdst[3] = 0;
    return 4;
#endif
}

// GPIO 초기화 함수 (원본 wizchip_qspi_pio.c의 pio_spi_gpio_setup 기반)
static int qspi_gpio_setup(qspi_test_state_t *state) {
    // gpiod 칩 열기
    state->chip = gpiod_chip_open_by_number(0);
    if (!state->chip) {
        fprintf(stderr, "gpiod_chip_open_by_number 실패\n");
        return -1;
    }
    
    // CS 핀 설정 (CPU 제어)
    state->cs_line = gpiod_chip_get_line(state->chip, state->spi_config->cs_pin);
    if (!state->cs_line || gpiod_line_request_output(state->cs_line, "qspi_cs", 1) < 0) {
        fprintf(stderr, "CS 핀 설정 실패\n");
        return -1;
    }
    
    // 데이터 핀 4개 설정 (초기에는 CPU에서 제어, 나중에 PIO로 전환)
    for (int i = 0; i < 4; i++) {
        int pin = state->spi_config->data_io0_pin + i;
        state->data_lines[i] = gpiod_chip_get_line(state->chip, pin);
        if (!state->data_lines[i] || gpiod_line_request_output(state->data_lines[i], "qspi_data", 0) < 0) {
            fprintf(stderr, "데이터 핀 %d 설정 실패\n", pin);
            return -1;
        }
    }
    
    printf("GPIO 설정 완료 - CS: %d, DATA: %d-%d, CLK: %d\n", 
           state->spi_config->cs_pin,
           state->spi_config->data_io0_pin, 
           state->spi_config->data_io3_pin,
           state->spi_config->clock_pin);
    
    return 0;
}

// PIO 초기화 함수 (원본 wizchip_qspi_pio.c의 wiznet_spi_pio_open 기반)
static int qspi_pio_setup(qspi_test_state_t *state) {
    // PIO 열기
    state->pio = pio_open(0);
    if (state->pio == NULL) {
        fprintf(stderr, "PIO 열기 실패\n");
        return -1;
    }
    
    // SM 할당
    state->pio_sm = pio_claim_unused_sm(state->pio, true);
    if (state->pio_sm < 0) {
        fprintf(stderr, "SM 할당 실패\n");
        pio_close(state->pio);
        return -1;
    }
    
    // QSPI Quad 프로그램 로드
    state->pio_offset = pio_add_program(state->pio, &wizchip_pio_spi_quad_write_read_program);
    if (state->pio_offset < 0) {
        fprintf(stderr, "PIO 프로그램 로드 실패\n");
        pio_sm_unclaim(state->pio, state->pio_sm);
        pio_close(state->pio);
        return -1;
    }
    
    printf("PIO 설정 완료 - PIO0, SM %d, 오프셋 %d\n", state->pio_sm, state->pio_offset);
    
    // SM 설정 (원본 wizchip_qspi_pio.c 기반)
    pio_sm_config c = pio_get_default_sm_config();
    
    // Wrap 설정 (PIO 프로그램 루프)
    sm_config_set_wrap(&c, state->pio_offset + wizchip_pio_spi_quad_write_read_wrap_target, 
                          state->pio_offset + wizchip_pio_spi_quad_write_read_wrap);
    
    // Quad 모드 핀 설정
    sm_config_set_out_pins(&c, state->spi_config->data_io0_pin, 4);    // 4비트 출력
    sm_config_set_in_pins(&c, state->spi_config->data_io0_pin);        // 4비트 입력  
    sm_config_set_set_pins(&c, state->spi_config->data_io0_pin, 4);    // 4비트 SET
    
    // 사이드셋 (CLK) 설정
    sm_config_set_sideset(&c, 1, false, false);
    sm_config_set_sideset_pins(&c, state->spi_config->clock_pin);
    
    // 시프트 설정
    sm_config_set_in_shift(&c, false, true, 8);   // MSB first, autopush at 8 bits
    // sm_config_set_out_shift(&c, true, , 8);  // MSB first, autopull at 8 bits
    
    // 클럭 분주 설정 (원본 참조)
    float clkdiv = (float)state->spi_config->clock_div_major + 
                   ((float)state->spi_config->clock_div_minor / 256.0f);
    sm_config_set_clkdiv(&c, clkdiv);
    
    printf("클럭 분주: %.2f (%.2f MHz)\n", clkdiv, 125.0f / clkdiv);
    
    // 핀을 PIO 제어로 전환
    for (int i = 0; i < 4; i++) {
        pio_gpio_init(state->pio, state->spi_config->data_io0_pin + i);
    }
    pio_gpio_init(state->pio, state->spi_config->clock_pin);
    
    // 핀 방향 설정 (출력)
    pio_sm_set_consecutive_pindirs(state->pio, state->pio_sm, 
                                   state->spi_config->data_io0_pin, 4, true);
    pio_sm_set_consecutive_pindirs(state->pio, state->pio_sm, 
                                   state->spi_config->clock_pin, 1, true);
    
    // SM 설정 적용
    pio_sm_init(state->pio, state->pio_sm, state->pio_offset, &c);
    
    return 0;
}

// QSPI 전송 함수 (원본 wizchip_qspi_pio.c 기반)
static void qspi_transfer(qspi_test_state_t *state, const uint8_t *tx_data, size_t len) {
    // CS LOW (선택)
    gpiod_line_set_value(state->cs_line, 0);
    usleep(1);  // CS setup time
    
    // SM 비활성화 후 재설정
    pio_sm_set_enabled(state->pio, state->pio_sm, false);
    
    // FIFO 클리어
    pio_sm_clear_fifos(state->pio, state->pio_sm);
    
    // 핀 방향을 출력으로 설정
    uint32_t pin_mask = 0xF << state->spi_config->data_io0_pin;
    pio_sm_set_pindirs_with_mask(state->pio, state->pio_sm, pin_mask, pin_mask);
    
    // SM 재시작
    pio_sm_restart(state->pio, state->pio_sm);
    pio_sm_clkdiv_restart(state->pio, state->pio_sm);
    
    // X 레지스터: 비트 카운트 (8비트 - 1)
    pio_sm_put(state->pio, state->pio_sm, 7);
    pio_sm_exec(state->pio, state->pio_sm, pio_encode_out(pio_x, 32));
    
    // Y 레지스터: 바이트 카운트 (len - 1)  
    pio_sm_put(state->pio, state->pio_sm, len - 1);
    pio_sm_exec(state->pio, state->pio_sm, pio_encode_out(pio_y, 32));
    
    // 프로그램 시작점으로 점프
    pio_sm_exec(state->pio, state->pio_sm, pio_encode_jmp(state->pio_offset));
    
    // SM 활성화
    pio_sm_set_enabled(state->pio, state->pio_sm, true);
    
    // 데이터 전송
    for (size_t i = 0; i < len; i++) {
        pio_sm_put_blocking(state->pio, state->pio_sm, tx_data[i]);
        printf("TX[%zu]: 0x%02X\n", i, tx_data[i]);
    }
    
    // 전송 완료 대기
    usleep(100);
    
    // SM 비활성화
    pio_sm_set_enabled(state->pio, state->pio_sm, false);
    
    // CS HIGH (해제)
    usleep(1);  // 데이터 홀드 시간
    gpiod_line_set_value(state->cs_line, 1);
    usleep(10); // CS deselect time
}

// 정리 함수
static void qspi_cleanup(qspi_test_state_t *state) {
    if (state->pio) {
        pio_sm_set_enabled(state->pio, state->pio_sm, false);
        pio_remove_program(state->pio, &wizchip_pio_spi_quad_write_read_program, state->pio_offset);
        pio_sm_unclaim(state->pio, state->pio_sm);
        pio_close(state->pio);
    }
    
    if (state->cs_line) {
        gpiod_line_release(state->cs_line);
    }
    
    for (int i = 0; i < 4; i++) {
        if (state->data_lines[i]) {
            gpiod_line_release(state->data_lines[i]);
        }
    }
    
    if (state->chip) {
        gpiod_chip_close(state->chip);
    }
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
    sm_config_set_clkdiv(&c, 250);               // 더 빠른 클럭으로 변경 (2.5MHz)

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

    
    // 테스트 데이터 패턴들  
    // uint8_t test_patterns[][8] = {
    //     {0xFF, 0xFF, 0x00, 0x00},           // Read command
    //     {0xFF, 0x00, 0x10, 0x00},           // Fast read
    //     {0xFF, 0x00, 0x20, 0xAA},           // Write command  
    //     {0xFF},                             // Read ID
    //     {0xFF, 0x00, 0x55, 0xAA},           // Pattern test
    //     {0xFF, 0x00, 0x00, 0x00, 0x12, 0x00, 0x00, 0x12, 0x34, 0x56, 0x78,0x34, 0x56,0xFF, 0x00, 0x00, 0x00, 0x12, 0x34, 0x56, 0x78,0x34, 0x56, 0x78,0x34, 0x56, 0x78,0x34, 0x56, 0x78}  // Quad read with data
    // };
     uint8_t test_patterns[32] =    {0x88, 0xff, 0xff, 0xff, 
                                    0x02, 0xFF, 0xff, 0xff,
                                    0xff, 0x56, 0x78,0x34, 
                                    0x56,0xff, 0x00, 0x00, 
                                    0xff, 0x12, 0x34, 0x56,
                                    0x78,0x34, 0x56, 0x78,
                                    0x34, 0x56, 0x78,0x34,
                                    0x34, 0x56, 0x78,0x34,
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


    // for (int i = 0xffff; i > 0; i--) {
    // // 데이터 전송 (각 바이트별로)
    //     pio_sm_put_blocking(pio, sm, i );
    //     printf("TX[%zu]", i);
    //     usleep(1000);
    // }
    //     usleep(1000000);
    //     usleep(1000000);
    //     usleep(1000000);
    // QSPI 테스트 루프
    while (keep_running) {
        

        // CS low (칩 선택)
        gpiod_line_set_value(cs_line, 0);

        // SM 비활성화하고 재설정
        pio_sm_set_enabled(pio, sm, false);
        
        // FIFO 클리어
        pio_sm_clear_fifos(pio, sm);
        
        // 핀 방향을 출력으로 설정 (Quad 모드)
        // uint32_t pin_mask = (1u << gpio_base) | (1u << (gpio_base+1)) | 
        // (1u << (gpio_base+2)) | (1u << (gpio_base+3));
        // pio_sm_set_pindirs_with_mask(pio, sm, pin_mask, pin_mask);



        

        // 헤더 바이트들(command_buf, command_len)을 DMA 경로로 PIO TX로 보냄

        // PIO 재시작
        pio_sm_restart(pio, sm);
        pio_sm_clkdiv_restart(pio, sm);
        // 🔧 올바른 x 레지스터 설정: 9개 워드 = 36바이트 = 72니블
        size_t total_bytes = sizeof(test_patterns);  // 36바이트
        size_t nibble_count = total_bytes * 2;       // 72니블 (Quad 모드)
        pio_sm_exec(pio, sm, pio_encode_set(pio_x, nibble_count - 1));  // 71
        
     
        
        // 🔧 DMA 설정: 9개 워드, 4바이트 단위
        pio_sm_set_enabled(pio, sm, true);                 
        pio_sm_clear_fifos(pio, sm);

        pio_sm_config_xfer(pio, sm, PIO_DIR_TO_SM, 512, 1);  // 9개, 4바이트 단위

        pio_sm_exec(pio, sm, pio_encode_set(pio_y, 0));
        pio_sm_exec(pio, sm, pio_encode_pull(false, true));
        
        int sent = pio_sm_xfer_data(pio, sm, PIO_DIR_TO_SM, 8, test_patterns); 
       
        // pio_sm_clear_fifos(pio, sm);
        // pio_sm_exec(pio, sm, pio_encode_set(pio_y, 0));
       
        //  pio_sm_xfer_data(pio, sm, PIO_DIR_TO_SM, 8, test_patterns); 


        // // pio_sm_exec(pio, sm, pio_encode_set(pio_y, 0));     // 고정값으로 
     
        // printf("보낸 바이트 수: %d\n", sent);

        // 잠시 대기
        usleep(10000);
        
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
