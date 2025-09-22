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
#include "Ethernet/socket.h"
#include "pio_func.h"

// struct pio_struct_Lihan pio_struct ;

// QSPI 모드 정의 (wizchip_qspi_pio.h와 호환) 
#define QSPI_SINGLE_MODE    1
#define QSPI_DUAL_MODE      2
#define QSPI_QUAD_MODE      4


#define _W6300_SPI_OP_          _WIZCHIP_SPI_VDM_OP_
#define _W6300_SPI_READ_                  (0x00 << 5)  |  0x80     ///< SPI interface Read operation in Control Phase
#define _W6300_SPI_WRITE_                 (0x01 << 5)  |  0x80     ///< SPI interface Write operation in Control Phase




static volatile int keep_running = 1;

uint8_t tx_buf[16];
uint32_t rx_buf[128] = {0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff};  ;


int main(int argc, char *argv[]) {
    uint16_t ADDR =  0X4138; //SiPR 
    uint32_t test_value[16] = {0x12000000,0x48000000,0x12000000,0x48000000,};


    /* 강제종료를 막고 안전한 자원해제를 위함 */
    signal(SIGINT, signal_handler); // Ctrl+C 시그널 처리
    signal(SIGTERM, signal_handler);// 종료 시그널 처리 
    
    
    struct gpiod_chip *chip;
    chip = gpiod_chip_open_by_number(0);
    if (!chip) {
        fprintf(stderr, "gpiod_chip_open_by_number 실패\n");
        exit(1);
    }
    
    struct gpiod_line *cs_line = gpiod_chip_get_line(chip, QSPI_CS_PIN);
    if (!cs_line || gpiod_line_request_output(cs_line, "qspi_cs", 1) < 0) {
        fprintf(stderr, "CS 핀 초기화 실패\n");
        gpiod_chip_close(chip);
        exit(1);
    }
    // return cs_line;
   
    pio_open_lihan(&pio_struct);

    printf("CS 핀 , PIO 초기화 완료\n");

    uint32_t cmdBuf[16] = {0,};
    // wiznet_spi_pio_read_byte(0xFF, 0x0000, cmdBuf, 64); // Quad Read 명령어

    printf("CS 핀 , PIO 초기화 완료\n");

    while (keep_running) {

        /*write SIPR[0:4] */
        gpiod_line_set_value(cs_line, 0);

        usleep(100);
        wiznet_spi_pio_write_byte( _W6300_SPI_WRITE_, ADDR, test_value, 4);
        usleep(100);
        gpiod_line_set_value(cs_line, 1);
        usleep(100);

        /*Read SIPR[0:4] */
        gpiod_line_set_value(cs_line, 0);
        usleep(100);
        wiznet_spi_pio_read_byte( _W6300_SPI_READ_, ADDR, rx_buf, 4);
        usleep(100);
        gpiod_line_set_value(cs_line, 1);

        /*verify result*/
#if 1
         for(int i=0; i<16; i++) {
             printf("%02X ", rx_buf[i]);
         }
         printf("\r\n");
#endif 
         

        usleep(200);
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