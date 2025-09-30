#include "pio_func.h"

struct pio_struct_Lihan pio_struct; // <-- 이 줄 추가


void convert32to8(const uint32_t *src, uint8_t *dst, size_t count) {
    for (size_t i = 0; i < count; i++) {
        dst[i] = (uint8_t)(src[i] & 0xFF);
    }
}
void convert8to32(const uint8_t *src, uint32_t *dst, size_t count) {
    for (size_t i = 0; i < count; i++) {
        dst[i] = ((uint32_t)src[i] );  // 상위 바이트는 자동으로 0
    }
}


/* func */
struct gpiod_line *cs_pin_init(struct gpiod_chip *chip) {
    // struct gpiod_chip *chip;
    // gpiod 초기화 (모든 핀을 libgpiod로 제어)
    // chip = gpiod_chip_open_by_number(0);
    // if (!chip) {
    //     fprintf(stderr, "gpiod_chip_open_by_number 실패\n");
    //     exit(1);
    // }
    
    // CS 핀
    struct gpiod_line *cs_line = gpiod_chip_get_line(chip, QSPI_CS_PIN);
    if (!cs_line || gpiod_line_request_output(cs_line, "qspi_cs", 1) < 0) {
        fprintf(stderr, "CS 핀 초기화 실패\n");
        gpiod_chip_close(chip);
        exit(1);
    }
    return cs_line;
}

int pio_open_lihan(struct pio_struct_Lihan *pioStruct) {
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
    sm_config_set_wrap(&pioStruct->c, pioStruct->offset, pioStruct->offset + wizchip_pio_spi_quad_write_read_wrap);  // wrap 설정 (상수로 고정)


    // #include "hardware/clocks.h"
    uint32_t sys_hz = clock_get_hz(clk_sys);
    printf("System Clock: %d Hz\n", sys_hz);
    // Quad SPI를 위한 추가 설정
    sm_config_set_out_pins(&pioStruct->c, QSPI_DATA_IO0_PIN, 4);    // 데이터 핀: GPIO 20-23
    sm_config_set_in_pins(&pioStruct->c, QSPI_DATA_IO0_PIN);        // 입력 핀
    sm_config_set_set_pins(&pioStruct->c, QSPI_DATA_IO0_PIN, 4);    // set pins도 데이터 핀으로 설정
    sm_config_set_clkdiv(&pioStruct->c, CLKDIV);               // 더 빠른 클럭으로 변경 (2.5MHz)

    sm_config_set_sideset(&pioStruct->c, 1, false, false);  // CLK를 sideset으로 사용
    sm_config_set_sideset_pins(&pioStruct->c, QSPI_CLOCK_PIN);    // CLK 핀 설정
    
    sm_config_set_in_shift(&pioStruct->c, false, true, 8);
    sm_config_set_out_shift(&pioStruct->c, true, true, 8);// 4바이트씩 shift

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

void pio_init_lihan(struct pio_struct_Lihan *pioStruct, bool enable , uint32_t tx_size   ,uint32_t rx_size) {

    // size_t send_size = 32;
    // size_t total_bytes = send_size ; 
    // size_t nibble_count = send_size * 2;       // 72니블 (Quad 모드)

    if (enable) {
        // SM 비활성화하고 재설정
        pio_sm_set_enabled(pioStruct->pio, pioStruct->sm, false);

        // FIFO 클리어
        pio_sm_clear_fifos(pioStruct->pio, pioStruct->sm);

        // PIO 재시작
        pio_sm_restart(pioStruct->pio, pioStruct->sm);
        pio_sm_clkdiv_restart(pioStruct->pio, pioStruct->sm);
                // X레지스터 길이 지정 - X는  Max loop count        

        pio_sm_config_xfer(pioStruct->pio, pioStruct->sm,  PIO_DIR_TO_SM, 512,2);  // 9개, 4바이트 단위
        if (rx_size > 0) {

        pio_sm_config_xfer(pioStruct->pio, pioStruct->sm, PIO_DIR_FROM_SM, 512, 4);  // 9개, 4바이트 단위
        }

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


        for (int i = 0; i < 4; i++)
        {
            gpio_set_pulls(QSPI_DATA_IO0_PIN + i, true, true);
            gpio_set_input_enabled(QSPI_DATA_IO0_PIN + i, true);
        }
        
        pio_sm_put_blocking(pioStruct->pio, pioStruct->sm, (tx_size *2  ) -1  );               // TX FIFO <= 값
        pio_sm_exec(pioStruct->pio, pioStruct->sm, pio_encode_pull(false, true));     // OSR <= TX FIFO
        pio_sm_exec(pioStruct->pio, pioStruct->sm, pio_encode_mov(pio_x, pio_osr));   // X <= OSR
        pio_sm_exec(pioStruct->pio, pioStruct->sm, pio_encode_out(pio_null, 32));     // OSR의 32비트를 그냥 폐기

        if (rx_size > 0) {

            // pio_sm_exec(pio_struct.pio, pio_struct.sm,pio_encode_jmp(pio_struct.offset+5));   // offset == 0번지
            pio_sm_put_blocking(pioStruct->pio, pioStruct->sm, (rx_size*2) -1 );               // TX FIFO <= 값
            pio_sm_exec(pioStruct->pio, pioStruct->sm, pio_encode_pull(false, true));     // OSR <= TX FIFO
            pio_sm_exec(pioStruct->pio, pioStruct->sm, pio_encode_mov(pio_y, pio_osr));   // X <= OSR

            pio_sm_exec(pioStruct->pio, pioStruct->sm, pio_encode_out(pio_null, 32));     // OSR의 32비트를 그냥 폐기
        }else{
            pio_sm_put_blocking(pioStruct->pio, pioStruct->sm, 0 );               // TX FIFO <= 값
            pio_sm_exec(pioStruct->pio, pioStruct->sm, pio_encode_pull(false, true));     // OSR <= TX FIFO
            pio_sm_exec(pioStruct->pio, pioStruct->sm, pio_encode_mov(pio_y, pio_osr));   // X <= OSR

            pio_sm_exec(pioStruct->pio, pioStruct->sm, pio_encode_out(pio_null, 32));     // OSR의 32비트를 그냥 폐기

        }



        pio_sm_exec(pioStruct->pio, pioStruct->sm, pio_encode_jmp(pioStruct->offset));
        //SM 활성화
        pio_sm_clear_fifos(pioStruct->pio, pioStruct->sm);

        // pio_sm_set_enabled(pioStruct->pio, pioStruct->sm, true);
        /*S
        이때부터 클럭 생성시작.....
        */
        // FIFO 클리어
        
     
    } else {
           // SM 활성화
        pio_sm_set_enabled(pioStruct->pio, pioStruct->sm, false);
    }

}



void wiznet_spi_pio_read_byte(uint8_t op_code, uint16_t AddrSel, uint8_t *rx, uint16_t rx_length) {

    // convert8to32(rx, (uint8_t *)rx, rx_length); // 32비트 배열을 8비트 배열로 변환
    uint32_t rx_buf32[2048] = {0,};
    uint8_t cmd2[2048] ={0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0};
    pio_read_byte(&pio_struct, op_code, AddrSel, cmd2, rx_length);
    // convert32to8(rx_buf32, (uint8_t *)rx, rx_length); // 32비트 배열을 8비트 배열로 변환
    for(int i=0; i< rx_length; i++){
        rx[i] = (uint8_t)cmd2[i] & 0XFF;
    }
    

}
void wiznet_spi_pio_write_byte(uint8_t op_code, uint16_t AddrSel, uint8_t *tx, uint16_t tx_length) {
    uint32_t tx_convert32[2048] = {0,};
    uint8_t tx_convert8[2048] = {0,};
    for(int i=0; i< tx_length; i++){
        tx_convert8[i] = (tx[i]&0xf0)>>4 | (tx[i]&0x0f)<<4;
    }
    convert8to32(tx_convert8, (uint32_t *)tx_convert32, tx_length); // 32비트 배열을 8비트 배열로 변환
    pio_write_byte(&pio_struct, op_code, AddrSel, tx_convert32, tx_length);
    // convert32to8(tx_convert32, (uint8_t *)tx, tx_length); // 32비트 배열을 8비트 배열로 변환

}

__attribute__((optimize("O0")))
void pio_read_byte(struct pio_struct_Lihan *pioStruct, uint8_t op_code, uint16_t AddrSel, uint8_t *rx, uint16_t rx_length){

    uint32_t cmd[2048] ={0,};
    uint32_t cmd2[2048] ={0,};
    uint32_t recv_buf_32[2048] ={0,};
    
    uint8_t cmd_size = mk_cmd_buf_lihan(cmd, op_code, AddrSel);


    __asm__ __volatile__("" ::: "memory");  // 메모리 배리어

    pio_init_lihan(pioStruct, true, (uint32_t)cmd_size, rx_length     ); // 80바이트 전송 준비
    
    
    
    pio_sm_set_enabled(pioStruct->pio, pioStruct->sm,true);
    int sent =  pio_sm_xfer_data(pioStruct->pio, pioStruct->sm, PIO_DIR_TO_SM, cmd_size*4, cmd);
    int recv =  pio_sm_xfer_data(pioStruct->pio, pioStruct->sm, PIO_DIR_FROM_SM, rx_length *4, recv_buf_32);  // len은 4의배수만되네..    if (sent < 0) {
    usleep(10); // 데이터 수신 대기
    pio_sm_set_enabled(pioStruct->pio, pioStruct->sm,false);
    for(int i=0; i< rx_length *4; i++){
        rx[i] = (uint8_t)((recv_buf_32[i]) &0xff);
    }

    __asm__ __volatile__("" ::: "memory");  // 메모리 배리어

}

__attribute__((optimize("O0")))
void pio_write_byte(struct pio_struct_Lihan *pioStruct, uint8_t op_code, uint16_t AddrSel, uint32_t *tx, uint16_t tx_length){
    uint32_t cmd[2048] ={0,};
    uint32_t rx[16] ={0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0};

    uint32_t cmd_size = mk_cmd_buf_include_data(cmd, tx, op_code, AddrSel, tx_length);

    uint32_t cmd2[1024]= {NULL,};
   

    uint8_t rx_size = 0; 
    pio_init_lihan(pioStruct, true,  cmd_size ,rx_size);
    
    pio_sm_set_enabled(pioStruct->pio, pioStruct->sm, true);
    int sent =  pio_sm_xfer_data(pioStruct->pio, pioStruct->sm, PIO_DIR_TO_SM, cmd_size *4 , cmd );
    int recv =  pio_sm_xfer_data(pioStruct->pio, pioStruct->sm, PIO_DIR_FROM_SM, rx_size *4  ,rx);  
    usleep(5);
    pio_sm_set_enabled(pioStruct->pio, pioStruct->sm,false);


    // SM 비활성화
    // pio_init_lihan(pioStruct, false, 0 ,0); // 80바이트 전송 종료
}

static uint8_t mk_cmd_buf_lihan(uint32_t *pdst, uint8_t opcode, uint16_t addr) {
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
    pdst[0] = ((opcode >> 6 & 0x01) << (4)) | ((opcode >> 7 & 0x01) << (0));
    pdst[1] = ((opcode >> 4 & 0x01) << (4)) | ((opcode >> 5 & 0x01) << (0));
    pdst[2] = ((opcode >> 2 & 0x01) << (4)) | ((opcode >> 3 & 0x01) << (0));
    pdst[3] = ((opcode >> 0 & 0x01) << (4)) | ((opcode >> 1 & 0x01) << (0));

    pdst[4] = ((uint8_t)(addr >> 8) & 0xF0 )>>4 |((uint8_t)(addr >> 8) & 0xf ) << 4   ;
    pdst[5] = ((uint8_t)(addr >> 0) & 0xF0 )>>4 |((uint8_t)(addr >> 0) & 0xF ) << 4   ;
    pdst[6] = 0 << (0);


#if false
#endif
    return 6 + 1;
#endif
    return 0;
}



static uint16_t mk_cmd_buf_include_data(uint32_t *outbuf, 
                                        uint32_t *databuf, 
                                        uint8_t opcode, 
                                        uint16_t rag_addr,  
                                        uint16_t len_byte) 
{

    uint16_t cmd_len =   mk_cmd_buf_lihan(outbuf, opcode, rag_addr);
    memcpy(outbuf + cmd_len, databuf,len_byte *4 );
    return cmd_len + len_byte;
}