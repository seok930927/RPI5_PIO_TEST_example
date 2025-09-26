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
#include "wizchip_conf.h"
#include "wizchip_spi.h"
#include "loopback.h"


// struct pio_struct_Lihan pio_struct ;

// QSPI 모드 정의 (wizchip_qspi_pio.h와 호환) 
// #define QSPI_SINGLE_MODE    1
// #define QSPI_DUAL_MODE      2
// #define QSPI_QUAD_MODE      4


// #define _W6300_SPI_OP_          _WIZCHIP_SPI_VDM_OP_
// #define _W6300_SPI_READ_                  (0x00 << 5)  |  0x80     ///< SPI interface Read operation in Control Phase
// #define _W6300_SPI_WRITE_                 (0x01 << 5)  |  0x80     ///< SPI interface Write operation in Control Phase


static wiz_NetInfo g_net_info = {
    .mac = {0x00, 0x08, 0xDC, 0x12, 0x34, 0x56}, // MAC address
    .ip = {192, 168, 11, 2},                     // IP address
    .sn = {255, 255, 255, 0},                    // Subnet Mask
    .gw = {192, 168, 11, 1},                     // Gateway
    .dns = {8, 8, 8, 8},                         // DNS server
#if _WIZCHIP_ > W5500
    .lla = {
        0xfe, 0x80, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00,
        0x02, 0x08, 0xdc, 0xff,
        0xfe, 0x57, 0x57, 0x25
    },             // Link Local Address
    .gua = {
        0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00
    },             // Global Unicast Address
    .sn6 = {
        0xff, 0xff, 0xff, 0xff,
        0xff, 0xff, 0xff, 0xff,
        0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00
    },             // IPv6 Prefix
    .gw6 = {
        0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00
    },             // Gateway IPv6 Address
    .dns6 = {
        0x20, 0x01, 0x48, 0x60,
        0x48, 0x60, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x88, 0x88
    },             // DNS6 server
    .ipmode = NETINFO_STATIC_ALL
#else
    .dhcp = NETINFO_STATIC
#endif
};

/* Loopback */
static uint8_t g_tcp_server_buf[1024 *2] = {
    0,
};

static volatile int keep_running = 1;

uint8_t tx_buf[16];
uint32_t rx_buf[128] = {0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff};  ;

// struct gpiod_chip *chip;
// struct gpiod_line *cs_line;

// int main(int argc, char *argv[]) {
int main() {
    // printf("test start  \r\n");
    uint16_t ADDR =  0X4138; //SiPR 

    int retval = 0;

    /* 강제종료를 막고 안전한 자원해제를 위함 */
    // signal(SIGINT, signal_handler); // Ctrl+C 시그널 처리
    // signal(SIGTERM, signal_handler);// 종료 시그널 처리 
    
    

    // chip = gpiod_chip_open_by_number(0);
    // if (!chip) {
    //     fprintf(stderr, "gpiod_chip_open_by_number 실패\n");
    //     exit(1);
    // }
    
    // cs_line = gpiod_chip_get_line(chip, QSPI_CS_PIN);
    // if (!cs_line || gpiod_line_request_output(cs_line, "qspi_cs", 1) < 0) {
    //     fprintf(stderr, "CS 핀 초기화 실패\n");
    //     gpiod_chip_close(chip);
    //     exit(1);
    // }
    // return cs_line;
    pio_open_lihan(&pio_struct);
    wizchip_initialize();
    //    wizchip_reset() ;
    
    // while(1){
    //     // printf("cid = %02X \r\n", getCIDR());

    //     uint8_t ip[16] = {0x1, 0x2, 0x4, 0x8,0x1, 0x2, 0x4, 0x8,0x1, 0x2, 0x4, 0x8,0x1, 0x2, 0x4, 0x8};
    //     setGUAR(ip);
    //     usleep(100);
    //     uint8_t getip[16]= {0xf00f0fff,}; 
    //     getGUAR(getip);
    //     printf("SIPR: %04X %04X %04X %04X %04X %04X %04X %04X %04X %04X %04X %04X %04X %04X %04X %04X\n", getip[0], getip[1], getip[2], getip[3], getip[4], getip[5], getip[6], getip[7], getip[8], getip[9], getip[10], getip[11], getip[12], getip[13], getip[14], getip[15]);
    //     // sleep(1);
    //     if(keep_running == 0) return 0  ; 

    // }


    printf("PIO 및 SM 초기화 완료\n");
    // reg_wizchip_qspi_cbfunc(wiznet_spi_pio_read_byte, wiznet_spi_pio_write_byte);
    printf("QSPI 콜백 함수 등록 완료\n");
    printf("QSPI 콜백 함수 등록 완료\n");

    network_initialize(g_net_info);

    print_network_information(g_net_info);

    while (keep_running) {


                /* TCP server loopback test */
        if ((retval = loopback_tcps(0, g_tcp_server_buf, 5000)) < 0) {
            printf(" loopback_tcps error : %d\n", retval);

            while (1)
                ;
        }
        /*write SIPR[0:4] */
        // gpiod_line_set_value(cs_line, 0);
        // printf( " id = %08x\n", getCIDR());
        // printf( " id = %08x\n", WIZCHIP_READ(0x00<< 8));
        // printf( " id = %08x\n", WIZCHIP_READ(0x01<< 8 ));
        // printf( " id = %08x\n", WIZCHIP_READ(0x02<< 8 ));
        // printf( " id = %08x\n", WIZCHIP_READ(0x03<< 8 ));
        // printf( " id = %08x\n", WIZCHIP_READ(0x04<< 8 ));
        // printf( " id = %08x\n", WIZCHIP_READ(0x05<< 8 ));
        // printf( " id = %08x\n", WIZCHIP_READ(0x06<< 8 ));


        // /* write Buffer TEST*/
    //     uint8_t test_value[4] = {0x12,0x48,0x12,0x48};
    //     setSIPR(test_value);
    //     usleep(100);
    //     // /*READ BUFFER TEST */
    //     uint8_t sipr[4] = {0,};
    //     getSIPR(sipr);
    //     printf("SIPR: %02X %02X %02X %02X\n", sipr[0], sipr[1], sipr[2], sipr[3]);
    // //    wizchip_reset() ;

    //     getSIPR(sipr);
    //     printf("SIPR: %02X %02X %02X %02X\n", sipr[0], sipr[1], sipr[2], sipr[3]);
    //     usleep(1000000);


        /*verify result*/
#if 0
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
    // gpiod_line_release(cs_line);
    // gpiod_chip_close(chip);
    printf("완료\n");
    return 0;

}
// Main While loop 종료 핸들러
void signal_handler(int sig) {
    (void)sig;
    printf("\n시그널 받음, 종료 중...\n");
    keep_running = 0;
}