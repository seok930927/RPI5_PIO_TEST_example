#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "../utils/piolib/include/piolib.h"

// 간단한 0~1 카운터 PIO 어셈블리 (FIFO 사용 안함)
static const uint16_t counter_program_instructions[] = {
    // 값 0 (0) 출력 - 8비트 모두 LOW
    0xe001,  // set pins, 0       - LOW
    0xa0ff,  // nop [255]         - 긴 지연
    0xe000,  // set pins, 0       - LOW
    0xa0ff,  // nop [255]         - 긴 지연
    0xa0ff,  // nop [255]         - 긴 지연
    0xa0ff,  // nop [255]         - 긴 지연
    0xa0ff,  // nop [255]         - 긴 지연
    0xa0ff,  // nop [255]         - 긴 지연
    0xa0ff,  // nop [255]         - 긴 지연
    0xa0ff,  // nop [255]         - 긴 지연

    // 값 1 (1) 출력 - bit 0만 HIGH  
    0xe001,  // set pins, 1       - HIGH
    0xa0ff,  // nop [255]         - 지연
    0xa0ff,  // nop [255]         - 지연
    0xe000,  // set pins, 1       - HIGH
    0xa0ff,  // nop [255]         - 지연
    0xa0ff,  // nop [255]         - 지연
    0xa0ff,  // nop [255]         - 지연
    0xa0ff,  // nop [255]         - 지연
    0xa0ff,  // nop [255]         - 지연
    0xa0ff,  // nop [255]         - 지연

    0x0000,  // jmp 0             - 처음으로 돌아가기
};

static const pio_program_t counter_program = {
    .instructions = counter_program_instructions,
    .length = 21,  // 간단한 명령어 개수
    .origin = -1,
};

// simple_toggle_test와 동일한 초기화 (set pins 사용)
void counter_program_init(PIO pio, uint sm, uint offset, uint pin) {
    pio_sm_config c = pio_get_default_sm_config();
    
    // Set pins 설정 (하드코딩 방식)
    sm_config_set_set_pins(&c, pin, 1);
    
    // 매우 느린 클럭 (simple_toggle_test와 동일)
    sm_config_set_clkdiv(&c, 125000000.0 / 2);  // 0.5Hz
    
    // GPIO 초기화
    pio_gpio_init(pio, pin);
    pio_sm_set_consecutive_pindirs(pio, sm, pin, 1, true);
    
    // State machine 시작
    pio_sm_init(pio, sm, offset, &c);
    pio_sm_set_enabled(pio, sm, true);
}

int main(int argc, const char **argv)
{
    PIO pio;
    int sm;
    uint offset;
    uint pin = 13;  // GPIO 13
    
    printf("=== 하드코딩 8비트 카운터 테스트 ===\n");
    printf("GPIO 13: 0~1 값들을 8비트로 출력 (간단한 하드코딩)\n");

    if (argc == 2)
        pin = (uint)strtoul(argv[1], NULL, 0);

    // simple_toggle_test와 완전히 동일한 순서
    pio = pio0;
    sm = pio_claim_unused_sm(pio, true);
    if (sm < 0) {
        printf("❌ State machine 할당 실패\n");
        return 1;
    }
    
    offset = pio_add_program(pio, &counter_program);
    printf("✅ 하드코딩 카운터 프로그램 로드됨 at %d, using sm %d, gpio %d\n", 
           offset, sm, pin);
    
    // FIFO 클리어 (simple_toggle_test와 동일)
    pio_sm_clear_fifos(pio, sm);
    
    // simple_toggle_test와 동일한 초기화
    counter_program_init(pio, sm, offset, pin);

    printf("\n🔄 하드코딩 카운터 동작 중... (Ctrl+C로 종료)\n");
    printf("로직 애널라이저로 GPIO %d 확인하세요!\n", pin);
    printf("패턴: 0(00000000) → 1(00000001) → 반복\n");
    
    // FIFO 사용 안함 - PIO가 완전 자동으로 동작
    while (1) {
        sleep(1);  // 그냥 대기만
    }

    return 0;
}
