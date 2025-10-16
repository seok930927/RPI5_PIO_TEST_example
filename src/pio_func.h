
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <string.h>
#include "piolib.h"
#include "wizchip_qspi_pio.pio.h"
#include <gpiod.h>
#include "Ethernet/socket.h"


// 핀 정의 (wizchip_qspi_pio.c와 호환)
#define QSPI_DATA_IO0_PIN   7
#define QSPI_DATA_IO1_PIN   21
#define QSPI_DATA_IO2_PIN   22
#define QSPI_DATA_IO3_PIN   23
#define QSPI_CLOCK_PIN      12
#define QSPI_CS_PIN         16
#define RESET_PIN           24

#define CLKDIV     5// 더 안전한 클럭 분주비로 증가

extern struct pio_struct_Lihan pio_struct ;

struct pio_struct_Lihan{
    PIO pio;
    int sm;
    pio_sm_config c;
    uint offset;    
};


static uint8_t mk_cmd_buf_lihan(uint8_t *pdst, uint8_t opcode, uint16_t addr) ;
void signal_handler(int sig);


void wiznet_spi_pio_read_byte( uint8_t op_code, uint16_t AddrSel, uint8_t *rx, uint16_t rx_length);
void wiznet_spi_pio_write_byte( uint8_t op_code, uint16_t AddrSel, uint8_t *tx, uint16_t tx_length);

void pio_read_byte(struct pio_struct_Lihan *pioStruct, uint8_t op_code, uint16_t AddrSel, uint8_t *rx, uint16_t rx_length);
void pio_write_byte(struct pio_struct_Lihan *pioStruct, uint8_t op_code, uint16_t AddrSel, uint32_t *tx, uint16_t tx_length);

static uint16_t mk_cmd_buf_include_data(uint8_t *outbuf,
                                        uint32_t *databuf, 
                                        uint8_t opcode, 
                                        uint16_t rag_addr,  
                                        uint16_t len_byte) ;


