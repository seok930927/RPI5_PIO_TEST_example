#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>

// PIO 완전 초기화 프로그램
// 모든 PIO 상태머신을 정지하고 GPIO를 초기화

#define PIO_BASE_ADDR 0x1f00050000ULL
#define GPIO_BASE_ADDR 0x1f000d0000ULL

// PIO 레지스터 오프셋
#define PIO_CTRL_OFFSET      0x000
#define PIO_SM0_CLKDIV_OFFSET 0x0c8
#define PIO_SM1_CLKDIV_OFFSET 0x0e0
#define PIO_SM2_CLKDIV_OFFSET 0x0f8
#define PIO_SM3_CLKDIV_OFFSET 0x110

// GPIO 레지스터 오프셋  
#define GPIO_CTRL_OFFSET     0x000

int reset_pio_hardware() {
    int fd;
    void *pio_map, *gpio_map;
    volatile uint32_t *pio_regs, *gpio_regs;
    
    printf("=== PIO 완전 초기화 시작 ===\n");
    
    // /dev/mem 열기
    fd = open("/dev/mem", O_RDWR | O_SYNC);
    if (fd < 0) {
        perror("❌ /dev/mem 열기 실패");
        return -1;
    }
    
    // PIO 메모리 매핑
    pio_map = mmap(NULL, 4096, PROT_READ | PROT_WRITE, MAP_SHARED, fd, PIO_BASE_ADDR);
    if (pio_map == MAP_FAILED) {
        perror("❌ PIO 메모리 매핑 실패");
        close(fd);
        return -1;
    }
    
    // GPIO 메모리 매핑
    gpio_map = mmap(NULL, 4096, PROT_READ | PROT_WRITE, MAP_SHARED, fd, GPIO_BASE_ADDR);
    if (gpio_map == MAP_FAILED) {
        perror("❌ GPIO 메모리 매핑 실패");
        munmap(pio_map, 4096);
        close(fd);
        return -1;
    }
    
    pio_regs = (volatile uint32_t *)pio_map;
    gpio_regs = (volatile uint32_t *)gpio_map;
    
    printf("✅ 메모리 매핑 성공\n");
    
    // 1. 모든 PIO 상태머신 정지 및 완전 리셋
    printf("🛑 모든 PIO 상태머신 강제 정지 중...\n");
    pio_regs[PIO_CTRL_OFFSET / 4] = 0x00000000;  // 모든 SM 비활성화
    usleep(10000);  // 10ms 대기
    
    // 2. PIO 명령어 메모리 완전 클리어 (0x000~0x020)
    printf("🧹 PIO 명령어 메모리 클리어 중...\n");
    for (int i = 0; i < 32; i++) {
        pio_regs[i] = 0x00000000;  // 모든 명령어 슬롯 클리어
        usleep(100);
    }
    
    // 3. 모든 상태머신 클럭 완전 리셋
    printf("🔄 PIO 클럭 완전 리셋 중...\n");
    pio_regs[PIO_SM0_CLKDIV_OFFSET / 4] = 0x00010000;  // SM0 클럭 리셋
    pio_regs[PIO_SM1_CLKDIV_OFFSET / 4] = 0x00010000;  // SM1 클럭 리셋
    pio_regs[PIO_SM2_CLKDIV_OFFSET / 4] = 0x00010000;  // SM2 클럭 리셋
    pio_regs[PIO_SM3_CLKDIV_OFFSET / 4] = 0x00010000;  // SM3 클럭 리셋
    usleep(10000);  // 10ms 대기
    
    // 4. 모든 FIFO 완전 클리어
    printf("💾 PIO FIFO 완전 클리어 중...\n");
    for (int i = 0x10; i < 0x20; i++) {
        pio_regs[i] = 0x00000000;  // FIFO 관련 레지스터 클리어
        usleep(100);
    }
    
    // 3. GPIO 핀들을 강제로 INPUT 모드로 설정 (GPIO 0-27)
    printf("📌 GPIO 핀 강제 리셋 중...\n");
    for (int gpio = 0; gpio < 28; gpio++) {
        // GPIO를 강제로 입력 모드로 설정하고 PIO에서 완전히 해제
        uint32_t gpio_ctrl_addr = GPIO_CTRL_OFFSET + (gpio * 8) + 4;
        gpio_regs[gpio_ctrl_addr / 4] = 0x00000005;  // GPIO 기본 기능으로 설정
        usleep(1000);  // 안정화 대기
        
        // 추가: 방향 레지스터도 강제로 INPUT으로 설정
        uint32_t gpio_dir_addr = GPIO_CTRL_OFFSET + (gpio * 8);
        gpio_regs[gpio_dir_addr / 4] = 0x00000000;  // INPUT 모드 강제 설정
        usleep(1000);
    }
    
    // 4. 특별히 GPIO 6번 핀 완전 리셋
    printf("🎯 GPIO 6번 핀 특별 리셋 중...\n");
    uint32_t gpio6_ctrl = GPIO_CTRL_OFFSET + (6 * 8) + 4;
    uint32_t gpio6_dir = GPIO_CTRL_OFFSET + (6 * 8);
    gpio_regs[gpio6_ctrl / 4] = 0x00000005;  // GPIO6 기본 기능
    gpio_regs[gpio6_dir / 4] = 0x00000000;   // GPIO6 INPUT 모드
    usleep(5000);  // 5ms 대기
    
    printf("✅ PIO 완전 초기화 완료!\n");
    printf("📝 모든 상태머신 강제 정지됨\n");
    printf("📝 PIO 명령어 메모리 완전 클리어됨\n"); 
    printf("📝 모든 FIFO 클리어됨\n");
    printf("📝 모든 GPIO 핀이 INPUT 모드로 강제 복구됨\n");
    printf("🎯 GPIO 6번 핀 특별 리셋 완료\n");
    
    // 메모리 해제
    munmap(pio_map, 4096);
    munmap(gpio_map, 4096);
    close(fd);
    
    return 0;
}

int main(int argc __attribute__((unused)), char *argv[] __attribute__((unused))) {
    printf("🔧 PIO 하드웨어 완전 초기화 도구\n");
    printf("   모든 PIO 상태머신과 GPIO를 리셋합니다\n\n");
    
    if (getuid() != 0) {
        printf("❌ 이 프로그램은 root 권한이 필요합니다.\n");
        printf("   sudo ./pio_reset 으로 실행하세요.\n");
        return 1;
    }
    
    int result = reset_pio_hardware();
    
    if (result == 0) {
        printf("\n🎉 PIO 초기화가 성공적으로 완료되었습니다!\n");
        printf("   이제 클럭 신호가 완전히 멈췄습니다.\n");
    } else {
        printf("\n💥 PIO 초기화 중 오류가 발생했습니다.\n");
    }
    
    return result;
}