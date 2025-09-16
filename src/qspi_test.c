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
#define QSPI_DATA_IO0_PIN   7
// #define QSPI_DATA_IO1_PIN   21
// #define QSPI_DATA_IO2_PIN   22
// #define QSPI_DATA_IO3_PIN   23
#define QSPI_CLOCK_PIN      12
#define QSPI_CS_PIN         16

// QSPI 모드 정의 (wizchip_qspi_pio.h와 호환) 
#define QSPI_SINGLE_MODE    1
#define QSPI_DUAL_MODE      2
#define QSPI_QUAD_MODE      4

#define CLKDIV              250     

// 현재 모드 설정
#define _WIZCHIP_QSPI_MODE_ QSPI_QUAD_MODE


struct pio_struct_Lihan{
    PIO pio;
    int sm;
    pio_sm_config c;
    uint offset;    
}pio_struct;

static volatile int keep_running = 1;
static uint16_t mk_cmd_buf(uint8_t *pdst, uint8_t opcode, uint16_t addr) ;
void signal_handler(int sig);

static uint16_t mk_cmd_buf_include_data(uint8_t *outbuf,
                                        uint8_t *databuf, 
                                        uint8_t opcode, 
                                        uint16_t rag_addr,  
                                        uint16_t len_byte) ;

uint8_t test_patterns[80] =    {
                                0x10, 0x20, 0x40, 0x80,
                                0x10, 0x20, 0x40, 0x80,
                                0x10, 0x20, 0x40, 0x80,
                                0x10, 0x20, 0x40, 0x80,
                                0x1, 0x2, 0x4, 0x8,
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
uint32_t test_patterns2[9] =    {
                                0x88ffffff, 
                                0x0202ffff,
                                0xff567834, 
                                0x56ff0000, 
                                0x00123456,
                                0x78345678,
                                0x34ff5678,
                                0x34ff5678,
                                0x56010101
                                } ; // Quad read with data

struct gpiod_line *cs_pin_init(struct gpiod_chip *chip) {
    // struct gpiod_chip *chip;
    // gpiod 초기화 (모든 핀을 libgpiod로 제어)
    chip = gpiod_chip_open_by_number(0);
    if (!chip) {
        fprintf(stderr, "gpiod_chip_open_by_number 실패\n");
        exit(1);
    }
    
    // CS 핀
    struct gpiod_line *cs_line = gpiod_chip_get_line(chip, QSPI_CS_PIN);
    if (!cs_line || gpiod_line_request_output(cs_line, "qspi_cs", 1) < 0) {
        fprintf(stderr, "CS 핀 초기화 실패\n");
        gpiod_chip_close(chip);
        exit(1);
    }
    return cs_line;
}

void pio_open_lihan(struct pio_struct_Lihan *pioStruct) {
    // PIO 초기화
    pioStruct->pio = pio_open(0);
    if (pioStruct->pio == NULL) {
        fprintf(stderr, "PIO 열기 실패\n");
        return 1;
    }

    pioStruct->sm = pio_claim_unused_sm(pioStruct->pio, true);
    if (pioStruct->sm < 0) {
        fprintf(stderr, "SM 할당 실패\n");
        pio_close(pioStruct->pio);
        return 1;
    }
    // wizchip_qspi_pio.pio.h의 QSPI Quad 프로그램 로드
    pioStruct->offset = pio_add_program(pioStruct->pio, &wizchip_pio_spi_quad_write_read_program);
    printf("QSPI Quad 프로그램이 오프셋 %d에 로드됨, SM %d 사용\n", pioStruct->offset, pioStruct->sm);

    // 기본 설정 가져오기
    pioStruct->c = pio_get_default_sm_config();
    sm_config_set_wrap(&pioStruct->c, pioStruct->offset, pioStruct->offset + 9);  // wrap 설정 (상수로 고정)

    // Quad SPI를 위한 추가 설정
    sm_config_set_out_pins(&pioStruct->c, QSPI_DATA_IO0_PIN, 4);    // 데이터 핀: GPIO 20-23
    sm_config_set_in_pins(&pioStruct->c, QSPI_DATA_IO0_PIN);        // 입력 핀
    sm_config_set_set_pins(&pioStruct->c, QSPI_DATA_IO0_PIN, 4);    // set pins도 데이터 핀으로 설정
    sm_config_set_clkdiv(&pioStruct->c, CLKDIV);               // 더 빠른 클럭으로 변경 (2.5MHz)

    sm_config_set_sideset(&pioStruct->c, 1, false, false);  // CLK를 sideset으로 사용
    sm_config_set_sideset_pins(&pioStruct->c, QSPI_CLOCK_PIN);    // CLK 핀 설정

    sm_config_set_in_shift(&pioStruct->c, true, true, 8);
    sm_config_set_out_shift(&pioStruct->c, true, true, 32);// 4바이트씩 shift

    // RP2350 스타일 PIO 설정 (QSPI Quad 모드)
    printf("\r\n[QSPI QUAD MODE]\r\n");
    
    // PIO 기능으로 GPIO 핀 설정
    for (int i = 0; i < 4; i++) {
        pio_gpio_init(pioStruct->pio, QSPI_DATA_IO0_PIN + i);
    }
    pio_gpio_init(pioStruct->pio, QSPI_CLOCK_PIN);
    // 핀 방향 설정 (출력으로)
    pio_sm_set_consecutive_pindirs(pioStruct->pio, pioStruct->sm, QSPI_DATA_IO0_PIN, 4, true);
    pio_sm_set_consecutive_pindirs(pioStruct->pio, pioStruct->sm, QSPI_CLOCK_PIN, 1, true);


    // 데이터 핀 풀다운 및 슈미트 트리거 활성화
    for (int i = 0; i < 4; i++) {
        gpio_set_pulls(QSPI_DATA_IO0_PIN + i, true, true);
        gpio_set_input_enabled(QSPI_DATA_IO0_PIN + i, true);
    }
    pio_sm_init(pioStruct->pio, pioStruct->sm, pioStruct->offset, &pioStruct->c);
}




void pio_init_lihan(struct pio_struct_Lihan *pioStruct, bool enable , uint32_t len_byte) {

    size_t send_size = 32;
    size_t total_bytes = send_size ; 
    size_t nibble_count = send_size * 2;       // 72니블 (Quad 모드)

    if (enable) {
        // SM 비활성화하고 재설정
        pio_sm_set_enabled(pioStruct->pio, pioStruct->sm, false);

        // FIFO 클리어
        pio_sm_clear_fifos(pioStruct->pio, pioStruct->sm);

        // PIO 재시작
        pio_sm_restart(pioStruct->pio, pioStruct->sm);
        pio_sm_clkdiv_restart(pioStruct->pio, pioStruct->sm);
                // X레지스터 길이 지정 - X는  Max loop count        

        #if 0
        //  pio_sm_exec(pio, sm, pio_encode_set(pio_x, ((nibble_count)- 1) ));  // 71
        //⚠️pio_encode_set() = 최대 31까지만 가능.
        #else
            sm_config_set_out_pins(&pioStruct->c, QSPI_DATA_IO0_PIN, 4);    // 데이터 핀: GPIO 20-23
    sm_config_set_in_pins(&pioStruct->c, QSPI_DATA_IO0_PIN);        // 입력 핀
    sm_config_set_set_pins(&pioStruct->c, QSPI_DATA_IO0_PIN, 4);    // set pins도 데이터 핀으로 설정
    // PIO 기능으로 GPIO 핀 설정
    for (int i = 0; i < 4; i++) {
        pio_gpio_init(pioStruct->pio, QSPI_DATA_IO0_PIN + i);
    }
    pio_gpio_init(pioStruct->pio, QSPI_CLOCK_PIN);
    // 핀 방향 설정 (출력으로)
    pio_sm_set_consecutive_pindirs(pioStruct->pio, pioStruct->sm, QSPI_DATA_IO0_PIN, 4, true);
    pio_sm_set_consecutive_pindirs(pioStruct->pio, pioStruct->sm, QSPI_CLOCK_PIN, 1, true);


        for (int i = 0; i < 4; i++) {
        gpio_set_pulls(QSPI_DATA_IO0_PIN + i, true, true);
        gpio_set_input_enabled(QSPI_DATA_IO0_PIN + i, true);
    }
        pio_sm_put_blocking(pioStruct->pio, pioStruct->sm, (16*2) -1  );               // TX FIFO <= 값
        pio_sm_exec(pioStruct->pio, pioStruct->sm, pio_encode_pull(false, true));     // OSR <= TX FIFO
        pio_sm_exec(pioStruct->pio, pioStruct->sm, pio_encode_mov(pio_x, pio_osr));   // X <= OSR
        pio_sm_exec(pioStruct->pio, pioStruct->sm, pio_encode_out(pio_null, 32));     // OSR의 32비트를 그냥 폐기
        #endif


        // pio_sm_exec(pio_struct.pio, pio_struct.sm,pio_encode_jmp(pio_struct.offset+5));   // offset == 0번지
        pio_sm_put_blocking(pioStruct->pio, pioStruct->sm,1 );               // TX FIFO <= 값
        pio_sm_exec(pioStruct->pio, pioStruct->sm, pio_encode_pull(false, true));     // OSR <= TX FIFO
        pio_sm_exec(pioStruct->pio, pioStruct->sm, pio_encode_mov(pio_y, pio_osr));   // X <= OSR
        pio_sm_exec(pioStruct->pio, pioStruct->sm, pio_encode_out(pio_null, 32));     // OSR의 32비트를 그냥 폐기
       
        pio_sm_exec(pioStruct->pio, pioStruct->sm, pio_encode_jmp(pioStruct->offset));
        //SM 활성화
        pio_sm_clear_fifos(pioStruct->pio, pioStruct->sm);

        pio_sm_set_enabled(pioStruct->pio, pioStruct->sm, true);
        /*
        이때부터 클럭 생성시작.....
        */
        // FIFO 클리어
        
     
    } else {
           // SM 활성화
        pio_sm_set_enabled(pioStruct->pio, pioStruct->sm, true);
    }

}

int main(int argc, char *argv[]) {
    /* 강제종료를 막고 안전한 자원해제를 위함 */
    signal(SIGINT, signal_handler); // Ctrl+C 시그널 처리
    signal(SIGTERM, signal_handler);// 종료 시그널 처리 
    
    
    struct gpiod_chip *chip;
    struct pio_struct_Lihan pio_struct ;
    // gpiod 초기화 (모든 핀을 libgpiod로 제어)

    struct gpiod_line *cs_line = cs_pin_init(chip);
    pio_open_lihan(&pio_struct);

    printf("CS 핀 , PIO 초기화 완료\n");

    uint8_t tx_buf[16];
    uint8_t rx_buf[128];

    while (keep_running) {
        // CS low (칩 선택)
        gpiod_line_set_value(cs_line, 0);

        
        pio_sm_config_xfer(pio_struct.pio, pio_struct.sm, PIO_DIR_TO_SM, 512,2);  // 9개, 4바이트 단위
        pio_sm_config_xfer(pio_struct.pio, pio_struct.sm, PIO_DIR_FROM_SM, 512, 2);  // 9개, 4바이트 단위

        pio_init_lihan(&pio_struct, true, 10); // 80바이트 전송 준비

        //DMA buffer 설정
        //Dma 버퍼에 데이터 전송
        uint8_t cmd_data_buf[500];
        uint16_t dataLen = mk_cmd_buf_include_data(cmd_data_buf, test_patterns, 0xaa, 0xBBBB, 80); // Quad Read 명령어와 주소 설정
        printf("Data Length to send = %d\n", dataLen);

        int sent =  pio_sm_xfer_data(pio_struct.pio, pio_struct.sm, PIO_DIR_TO_SM, 16 , test_patterns); // len은 4의배수만되네..
                    pio_sm_xfer_data(pio_struct.pio, pio_struct.sm, PIO_DIR_FROM_SM, 1 , rx_buf); // len은 4의배수만되네..


        for(int i=0; i<16; i++) {
            printf("%02X ", test_patterns[i]);
        }

        usleep(200);
        
        // SM 비활성화
        pio_init_lihan(&pio_struct, false, 0); // 80바이트 전송 종료
        
        // CS high (칩 선택 해제)
        gpiod_line_set_value(cs_line, 1);
        
        
    }
    
    printf("\n정리 중...\n");
    pio_sm_set_enabled(pio_struct.pio, pio_struct.sm, false);
    pio_remove_program(pio_struct.pio, &wizchip_pio_spi_quad_write_read_program, pio_struct.offset);
    pio_sm_unclaim(pio_struct.pio, pio_struct.sm);
    pio_close(pio_struct.pio);
    // gpiod 해제
    gpiod_line_release(cs_line);
    gpiod_chip_close(chip);
    printf("완료\n");
    return 0;

}
// Main While loop 종료 핸들러
void signal_handler(int sig) {
    (void)sig;
    printf("\n시그널 받음, 종료 중...\n");
    keep_running = 0;
}



static uint16_t mk_cmd_buf(uint8_t *pdst, uint8_t opcode, uint16_t addr) {
#if (_WIZCHIP_QSPI_MODE_ == QSPI_SINGLE_MODE)

    pdst[0] = opcode;
    pdst[1] = (uint8_t)((addr >> 8) & 0xFF);
    pdst[2] = (uint8_t)((addr >> 0) & 0xFF);
    pdst[3] = 0;

    return 3 + 1;
#elif (_WIZCHIP_QSPI_MODE_ == QSPI_DUAL_MODE)
    pdst[0] = ((opcode >> 7 & 0x01) << 6) | ((opcode >> 6 & 0x01) << 4) | ((opcode >> 5 & 0x01) << 2) | ((opcode >> 4 & 0x01) << 0);
    pdst[1] = ((opcode >> 3 & 0x01) << 6) | ((opcode >> 2 & 0x01) << 4) | ((opcode >> 1 & 0x01) << 2) | ((opcode >> 0 & 0x01) << 0);
    pdst[2] = (uint8_t)((addr >> 8) & 0xFF);
    pdst[3] = (uint8_t)((addr >> 0) & 0xFF);

    pdst[4] = 0;

    return 4 + 1;
#elif (_WIZCHIP_QSPI_MODE_ == QSPI_QUAD_MODE)
    pdst[0] = ((opcode >> 7 & 0x01) << 4) | ((opcode >> 6 & 0x01) << 0);
    pdst[1] = ((opcode >> 5 & 0x01) << 4) | ((opcode >> 4 & 0x01) << 0);
    pdst[2] = ((opcode >> 3 & 0x01) << 4) | ((opcode >> 2 & 0x01) << 0);
    pdst[3] = ((opcode >> 1 & 0x01) << 4) | ((opcode >> 0 & 0x01) << 0);

    pdst[4] = ((uint8_t)(addr >> 8) & 0xFF);
    pdst[5] = ((uint8_t)(addr >> 0) & 0xFF);

    pdst[6] = 0;

    return 6 + 1;
#endif
    return 0;
}





static uint16_t mk_cmd_buf_include_data(uint8_t *outbuf, 
                                        uint8_t *databuf, 
                                        uint8_t opcode, 
                                        uint16_t rag_addr,  
                                        uint16_t len_byte) {


    uint16_t cmd_len =   mk_cmd_buf(outbuf, opcode, rag_addr);

   memcpy(outbuf + cmd_len, databuf,len_byte );

    return cmd_len + len_byte;
}