#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>

#include "../../utils/piolib/include/piolib.h"

// ================================
// QSPI PIO 프로그램 (기본 구조)
// ================================

// 클럭 생성 PIO (50MHz, 50% 듀티)
static const uint16_t qspi_clock_instructions[] = {
    0xe001,  // set pins, 1 (HIGH) - 1 클럭
    0xa042,  // nop               - 1 클럭 (HIGH 유지)
    0xa042,  // nop               - 1 클럭 (HIGH 유지)
    0xe000,  // set pins, 0 (LOW)  - 1 클럭  
    0x0000,  // jmp 0 (처음으로)   - 1 클럭 (LOW 유지)
};

static const pio_program_t qspi_clock_program = {
    .instructions = qspi_clock_instructions,
    .length = 4,
    .origin = -1,
};

// TODO: 데이터 전송 PIO 프로그램 추가 예정
// static const uint16_t qspi_data_instructions[] = { ... };

// ================================
// GPIO 핀 정의
// ================================
#define QSPI_CLK_PIN    6   // 클럭 (50MHz)
#define QSPI_DQ0_PIN    7   // 데이터 0
#define QSPI_DQ1_PIN    8   // 데이터 1  
#define QSPI_DQ2_PIN    9   // 데이터 2
#define QSPI_DQ3_PIN    10  // 데이터 3
#define QSPI_CS_PIN     11  // Chip Select

// ================================
// QSPI 초기화 함수
// ================================
void qspi_clock_init(PIO pio, uint sm, uint offset, uint pin) {
    pio_gpio_init(pio, pin);
    pio_sm_set_consecutive_pindirs(pio, sm, pin, 1, true);

    pio_sm_config c = pio_get_default_sm_config();
    sm_config_set_set_pins(&c, pin, 1);
    
    float div = 1;  // 50MHz (최고 속도)
    sm_config_set_clkdiv(&c, div);

    pio_sm_init(pio, sm, offset, &c);
    pio_sm_set_enabled(pio, sm, true);
}

// ================================
// QSPI 테스트 함수
// ================================
void test_qspi_clock_only() {
    PIO pio = pio0;
    int sm = pio_claim_unused_sm(pio, true);
    uint offset = pio_add_program(pio, &qspi_clock_program);
    
    printf("📍 QSPI 클럭 테스트 - GPIO %d에서 50MHz 클럭 wdwdㄱㄷㄱ생성\n", QSPI_CLK_PIN);
    
    qspi_clock_init(pio, sm, offset, QSPI_CLK_PIN);
    
    printf("🔄 QSPI 클럭 출력 중... (10초 후 자동 종료)\n");
    printf("오실로스코프로 GPIO %d 확인하세요!\n", QSPI_CLK_PIN);
    
    sleep(10);  // 10초 동안 클럭 출력
    
    pio_sm_set_enabled(pio, sm, false);
    printf("✅ QSPI 클럭 테스트 완료\n");
}

void test_qspi_w5500() {
    printf("🚧 W5500 QSPI 통신 테스트 (구현 예정)\n");
    printf("핀 배치:\n");
    printf("  CLK: GPIO %d\n", QSPI_CLK_PIN);
    printf("  DQ0: GPIO %d\n", QSPI_DQ0_PIN);
    printf("  DQ1: GPIO %d\n", QSPI_DQ1_PIN);
    printf("  DQ2: GPIO %d\n", QSPI_DQ2_PIN);
    printf("  DQ3: GPIO %d\n", QSPI_DQ3_PIN);
    printf("  CS:  GPIO %d\n", QSPI_CS_PIN);
}

// ================================
// 메인 함수
// ================================
int main(int argc, const char **argv)
{
    printf("=== QSPI PIO 통신 테스트 ===\n");
    
    if (argc > 1 && strcmp(argv[1], "w5500") == 0) {
        test_qspi_w5500();
    } else {
        test_qspi_clock_only();
    }
    
    return 0;
}
