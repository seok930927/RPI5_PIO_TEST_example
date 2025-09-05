#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <stdint.h>
#include <gpiod.h>

// RPI5용 깔끔한 QSPI 테스트 - PIO 대신 libgpiod 사용

static volatile int keep_running = 1;

void signal_handler(int sig) {
    printf("\n시그널 받음, 종료 중...\n");
    keep_running = 0;
}

// GPIO 라인 요청 헬퍼 함수
static int request_gpio_line(struct gpiod_chip *chip, int gpio, struct gpiod_line **line_out, const char *consumer, int initial_value) {
    struct gpiod_line *line = gpiod_chip_get_line(chip, gpio);
    if (!line) {
        fprintf(stderr, "GPIO %d get failed\n", gpio);
        return -1;
    }
    
    if (gpiod_line_request_output(line, consumer, initial_value) < 0) {
        fprintf(stderr, "GPIO %d request_output failed\n", gpio);
        return -1;
    }
    
    *line_out = line;
    return 0;
}

// QSPI 전송 함수 (4비트 parallel)
void qspi_send_byte(struct gpiod_line *clk_line, struct gpiod_line *data_lines[4], uint8_t data) {
    // 8비트를 4비트씩 2번 전송 (MSB first)
    for (int nibble = 1; nibble >= 0; nibble--) {
        uint8_t val = (data >> (nibble * 4)) & 0x0F;
        
        // 4비트 데이터를 parallel로 출력
        for (int bit = 0; bit < 4; bit++) {
            int bit_val = (val >> bit) & 1;
            gpiod_line_set_value(data_lines[bit], bit_val);
        }
        
        // CLK 토글 (데이터 setup 후 클럭)
        gpiod_line_set_value(clk_line, 0);
        usleep(2);  // 클럭 half period
        gpiod_line_set_value(clk_line, 1);
        usleep(2);
        gpiod_line_set_value(clk_line, 0);
        usleep(2);
    }
}

int main(int argc, char *argv[]) {
    // GPIO 핀 번호 설정
    int gpio_base = 20;  // 데이터 핀: 20, 21, 22, 23
    int gpio_cs = 16;    // CS 핀
    int gpio_clk = 12;   // CLK 핀

    // 신호 핸들러 등록
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);

    printf("=== RPI5 QSPI 테스트 (libgpiod 기반) ===\n");
    printf("핀 구성: CS=%d, CLK=%d, DATA=%d~%d\n", gpio_cs, gpio_clk, gpio_base, gpio_base + 3);

    // gpiod 초기화
    struct gpiod_chip *chip = gpiod_chip_open_by_number(0);
    if (!chip) {
        fprintf(stderr, "gpiod_chip_open_by_number 실패\n");
        return 1;
    }

    // GPIO 라인들 요청
    struct gpiod_line *cs_line = NULL;
    struct gpiod_line *clk_line = NULL;
    struct gpiod_line *data_lines[4] = {NULL, NULL, NULL, NULL};

    // CS 핀 (idle high)
    if (request_gpio_line(chip, gpio_cs, &cs_line, "qspi_cs", 1) < 0) {
        goto cleanup;
    }

    // CLK 핀 (idle low)
    if (request_gpio_line(chip, gpio_clk, &clk_line, "qspi_clk", 0) < 0) {
        goto cleanup;
    }

    // 데이터 핀들 (초기값 0)
    for (int i = 0; i < 4; i++) {
        char consumer[32];
        snprintf(consumer, sizeof(consumer), "qspi_data%d", i);
        if (request_gpio_line(chip, gpio_base + i, &data_lines[i], consumer, 0) < 0) {
            goto cleanup;
        }
    }

    printf("GPIO 초기화 완료\n");
    printf("QSPI 전송 시작...\n");
    printf("로직 애널라이저로 파형을 확인하세요!\n");

    // QSPI 테스트 루프
    uint32_t transfer_count = 0;
    while (keep_running) {
        // CS assert (low)
        gpiod_line_set_value(cs_line, 0);
        usleep(10);  // CS setup time

        // 테스트 데이터 전송
        uint8_t test_data = 0xAB;  // 고정 패턴
        qspi_send_byte(clk_line, data_lines, test_data);

        printf("전송 #%u: 0x%02X\n", ++transfer_count, test_data);

        // CS deassert (high)
        usleep(10);  // data hold time
        gpiod_line_set_value(cs_line, 1);

        // 전송 간격
        usleep(500000);  // 0.5초
    }

cleanup:
    printf("\n정리 중...\n");

    // GPIO 라인들 해제
    if (cs_line) gpiod_line_release(cs_line);
    if (clk_line) gpiod_line_release(clk_line);
    for (int i = 0; i < 4; i++) {
        if (data_lines[i]) gpiod_line_release(data_lines[i]);
    }

    // chip 해제
    if (chip) gpiod_chip_close(chip);

    printf("완료\n");
    return 0;
}
