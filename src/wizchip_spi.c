/**
    Copyright (c) 2022 WIZnet Co.,Ltd

    SPDX-License-Identifier: BSD-3-Clause
*/

/**
    ----------------------------------------------------------------------------------------------------
    Includes
    ----------------------------------------------------------------------------------------------------
*/
#include <stdio.h>


#include "wizchip_conf.h"
#include "wizchip_spi.h"
#include "board_list.h"


#include "wizchip_qspi_pio.h"
#include "pio_func.h"
// #include "pico/stdlib.h"
// #include "pico/binary_info.h"
// #include "pico/critical_section.h"
// #include "hardware/dma.h"

/**
    ----------------------------------------------------------------------------------------------------
    Macros
    ----------------------------------------------------------------------------------------------------
*/

/**
    ----------------------------------------------------------------------------------------------------
    Variables
    ----------------------------------------------------------------------------------------------------
*/

#ifdef USE_SPI_DMA
static uint dma_tx;
static uint dma_rx;
static dma_channel_config dma_channel_config_tx;
static dma_channel_config dma_channel_config_rx;
#endif

#ifdef USE_PIO
#if   (_WIZCHIP_ == W6300)
wiznet_spi_config_t g_spi_config = {
    .clock_div_major = WIZNET_SPI_CLKDIV_MAJOR_DEFAULT,
    .clock_div_minor = WIZNET_SPI_CLKDIV_MINOR_DEFAULT,
    .clock_pin = PIO_SPI_SCK_PIN,
    .data_io0_pin = PIO_SPI_DATA_IO0_PIN,
    .data_io1_pin = PIO_SPI_DATA_IO1_PIN,
    .data_io2_pin = PIO_SPI_DATA_IO2_PIN,
    .data_io3_pin = PIO_SPI_DATA_IO3_PIN,
    .cs_pin = PIN_CS,
    .reset_pin = PIN_RST,
    .irq_pin = PIN_INT,
};
#else
wiznet_spi_config_t g_spi_config = {
    .data_in_pin = PIN_MISO,
    .data_out_pin = PIN_MOSI,
    .cs_pin = PIN_CS,
    .clock_pin = PIN_SCK,
    .irq_pin = PIN_INT,
    .reset_pin = PIN_RST,
    .clock_div_major = WIZNET_SPI_CLKDIV_MAJOR_DEFAULT,
    .clock_div_minor = WIZNET_SPI_CLKDIV_MINOR_DEFAULT,
};
#endif
#endif
wiznet_spi_handle_t spi_handle;
struct gpiod_chip *chip;
struct gpiod_line *cs_line;
struct gpiod_line *RST_line;




/**
    ----------------------------------------------------------------------------------------------------
    Functions
    ----------------------------------------------------------------------------------------------------
*/
static inline void wizchip_select(void) {
        usleep(10);
       gpiod_line_set_value(cs_line, 0);
}

static inline void wizchip_deselect(void) {
        usleep(10);
        gpiod_line_set_value(cs_line, 1);
}

void wizchip_reset() {
//will be defined later
       gpiod_line_set_value(RST_line, 0);
       usleep(500000);
       gpiod_line_set_value(RST_line, 1);
       usleep(100);


}

void wizchip_initialize(void) {

    printf("gpiod_chip_open_by_number 성공\n");
    chip = gpiod_chip_open_by_number(0);
    if (!chip) {
        fprintf(stderr, "gpiod_chip_open_by_number 실패\n");
        exit(1);
    }
    cs_line = gpiod_chip_get_line(chip, QSPI_CS_PIN);
    if (!cs_line || gpiod_line_request_output(cs_line, "qspi_cs", 1) < 0) {
        fprintf(stderr, "CS 핀 초기화 실패\n");
        gpiod_chip_close(chip);
        exit(1);
    }
    RST_line = gpiod_chip_get_line(chip, RESET_PIN);
    if (!RST_line || gpiod_line_request_output(RST_line, "w6300 RESET", 1) < 0) {
        fprintf(stderr, "RST 핀 초기화 실패\n");
        gpiod_chip_close(chip);
        exit(1);
    }

       wizchip_reset() ;
    printf("CS 핀 초기화 성공\n");

    reg_wizchip_qspi_cbfunc(wiznet_spi_pio_read_byte, wiznet_spi_pio_write_byte);
    reg_wizchip_cs_cbfunc(wizchip_select, wizchip_deselect);
    printf("QSPI 콜백 함수 등록 성공--3223223\n");

//     /* W5x00, W6x00 initialize */
//     uint8_t temp;
// #if (_WIZCHIP_ == W5100S)
//     uint8_t memsize[2][4] = {{2, 2, 2, 2}, {2, 2, 2, 2}};
// #elif (_WIZCHIP_ == W5500)
//     uint8_t memsize[2][8] = {{2, 2, 2, 2, 2, 2, 2, 2}, {2, 2, 2, 2, 2, 2, 2, 2}};
// #elif (_WIZCHIP_ == W6100)
//     uint8_t memsize[2][8] = {{2, 2, 2, 2, 2, 2, 2, 2}, {2, 2, 2, 2, 2, 2, 2, 2}};
// #elif (_WIZCHIP_ == W6300)
//     uint8_t memsize[2][8] = {{4, 4, 4, 4, 4, 4, 4, 4}, {4, 4, 4, 4, 4, 4, 4, 4}};
// #endif
//     usleep(1000000);
//     if (ctlwizchip(CW_INIT_WIZCHIP, (void *)memsize) == -1) {
// #if _WIZCHIP_ <= W5500
//         printf(" W5x00 initialized fail\n");
// #else
//         printf(" W6x00 initialized fail\n");
// #endif

//         return;
//     }

  
//     /* Check PHY link status */
//     do {
//         if (ctlwizchip(CW_GET_PHYLINK, (void *)&temp) == -1) {
//             printf(" Unknown PHY link status\n");

//             return;
//         }
//     } while (temp == PHY_LINK_OFF);

}


/* Network */
void network_initialize(wiz_NetInfo net_info) {
#if _WIZCHIP_ <= W5500
    ctlnetwork(CN_SET_NETINFO, (void *)&net_info);
#else
    uint8_t syslock = SYS_NET_LOCK;
    ctlwizchip(CW_SYS_UNLOCK, &syslock);
    ctlnetwork(CN_SET_NETINFO, (void *)&net_info);
#endif
}

void print_network_information(wiz_NetInfo net_info) {
    uint8_t tmp_str[8] = {
        0,
    };

    ctlnetwork(CN_GET_NETINFO, (void *)&net_info);
    ctlwizchip(CW_GET_ID, (void *)tmp_str);
#if _WIZCHIP_ <= W5500
    if (net_info.dhcp == NETINFO_DHCP) {
        printf("====================================================================================================\n");
        printf(" %s network configuration : DHCP\n\n", (char *)tmp_str);
    } else {
        printf("====================================================================================================\n");
        printf(" %s network configuration : static\n\n", (char *)tmp_str);
    }

    printf(" MAC         : %02X:%02X:%02X:%02X:%02X:%02X\n", net_info.mac[0], net_info.mac[1], net_info.mac[2], net_info.mac[3], net_info.mac[4], net_info.mac[5]);
    printf(" IP          : %d.%d.%d.%d\n", net_info.ip[0], net_info.ip[1], net_info.ip[2], net_info.ip[3]);
    printf(" Subnet Mask : %d.%d.%d.%d\n", net_info.sn[0], net_info.sn[1], net_info.sn[2], net_info.sn[3]);
    printf(" Gateway     : %d.%d.%d.%d\n", net_info.gw[0], net_info.gw[1], net_info.gw[2], net_info.gw[3]);
    printf(" DNS         : %d.%d.%d.%d\n", net_info.dns[0], net_info.dns[1], net_info.dns[2], net_info.dns[3]);
    printf("====================================================================================================\n\n");
#else
    printf("==========================================================\n");
    printf(" %s network configuration\n\n", (char *)tmp_str);

    printf(" MAC         : %02X:%02X:%02X:%02X:%02X:%02X\n", net_info.mac[0], net_info.mac[1], net_info.mac[2], net_info.mac[3], net_info.mac[4], net_info.mac[5]);
    printf(" IP          : %d.%d.%d.%d\n", net_info.ip[0], net_info.ip[1], net_info.ip[2], net_info.ip[3]);
    printf(" Subnet Mask : %d.%d.%d.%d\n", net_info.sn[0], net_info.sn[1], net_info.sn[2], net_info.sn[3]);
    printf(" Gateway     : %d.%d.%d.%d\n", net_info.gw[0], net_info.gw[1], net_info.gw[2], net_info.gw[3]);
    printf(" DNS         : %d.%d.%d.%d\n", net_info.dns[0], net_info.dns[1], net_info.dns[2], net_info.dns[3]);
    print_ipv6_addr(" GW6 ", net_info.gw6);
    print_ipv6_addr(" LLA ", net_info.lla);
    print_ipv6_addr(" GUA ", net_info.gua);
    print_ipv6_addr(" SUB6", net_info.sn6);
    print_ipv6_addr(" DNS6", net_info.dns6);
    printf("==========================================================\n\n");
#endif
}

void print_ipv6_addr(uint8_t* name, uint8_t* ip6addr) {
    printf("%s        : ", name);
    printf("%04X:%04X", ((uint16_t)ip6addr[0] << 8) | ((uint16_t)ip6addr[1]), ((uint16_t)ip6addr[2] << 8) | ((uint16_t)ip6addr[3]));
    printf(":%04X:%04X", ((uint16_t)ip6addr[4] << 8) | ((uint16_t)ip6addr[5]), ((uint16_t)ip6addr[6] << 8) | ((uint16_t)ip6addr[7]));
    printf(":%04X:%04X", ((uint16_t)ip6addr[8] << 8) | ((uint16_t)ip6addr[9]), ((uint16_t)ip6addr[10] << 8) | ((uint16_t)ip6addr[11]));
    printf(":%04X:%04X\r\n", ((uint16_t)ip6addr[12] << 8) | ((uint16_t)ip6addr[13]), ((uint16_t)ip6addr[14] << 8) | ((uint16_t)ip6addr[15]));
}