#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/mman.h>

// GPIO 직접 제어로 비트스트림 출력 테스트
#define GPIO_BASE   0x1f000d0000UL  // 라즈베리파이 5 GPIO 베이스
#define GPIO_SIZE   0x30000

volatile unsigned int *gpio_map;

void gpio_init() {
    int fd = open("/dev/gpiomem0", O_RDWR | O_SYNC);
    if (fd < 0) {
        perror("GPIO 접근 실패");
        exit(1);
    }
    
    gpio_map = (volatile unsigned int *)mmap(NULL, GPIO_SIZE, 
                                           PROT_READ | PROT_WRITE, 
                                           MAP_SHARED, fd, 0);
    close(fd);
    
    if (gpio_map == MAP_FAILED) {
        perror("GPIO 메모리 맵핑 실패");
        exit(1);
    }
}

void gpio_set_output(int pin) {
    int reg = pin / 10;
    int shift = (pin % 10) * 3;
    gpio_map[reg] = (gpio_map[reg] & ~(7 << shift)) | (1 << shift);
}

void gpio_write(int pin, int value) {
    if (value) {
        gpio_map[7] = 1 << pin;  // SET register
    } else {
        gpio_map[10] = 1 << pin; // CLR register  
    }
}

void output_bitstream() {
    const int pin = 13;
    gpio_set_output(pin);
    
    printf("GPIO %d에서 0~15 비트스트림 출력 시작...\n", pin);
    
    for (int value = 0; value < 16; value++) {
        printf("값 %d (0b%04b): ", value, value);
        
        // 8비트 LSB-first 출력
        for (int bit = 0; bit < 8; bit++) {
            int bit_value = (value >> bit) & 1;
            gpio_write(pin, bit_value);
            printf("%d", bit_value);
            usleep(100000); // 100ms 지연
        }
        printf("\n");
        usleep(500000); // 500ms 지연
    }
}

int main() {
    printf("=== GPIO 직접 제어 비트스트림 테스트 ===\n");
    
    gpio_init();
    output_bitstream();
    
    printf("테스트 완료!\n");
    return 0;
}
