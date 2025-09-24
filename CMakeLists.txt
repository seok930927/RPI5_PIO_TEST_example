cmake_minimum_required(VERSION 3.16)

project(qspi_pio)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

# PIOLib 경로 설정
set(PIOLIB_PATH "${CMAKE_CURRENT_SOURCE_DIR}/utils/piolib")

file(GLOB IO_LIB_SRC lib/ioLibrary_Driver/Ethernet/*.c)
file(GLOB W6300_SRC lib/ioLibrary_Driver/Ethernet/W6300/*.c)
file(GLOB LIB_loopback lib/ioLibrary_Driver/Application/loopback/*.c)
# 실행 파일 생성
add_executable(qspi_test 
            src/qspi_test.c 
            src/pio_func.c 
            src/wizchip_spi.c
            include/wizchip_qspi_pio.pio.c 
            ${IO_LIB_SRC} 
            ${LIB_loopback} 
            ${W6300_SRC})

# PIOLib 헤더 포함
target_include_directories(qspi_test PRIVATE 
    ${PIOLIB_PATH}/include
    include
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    lib/ioLibrary_Driver/
    lib/ioLibrary_Driver/Ethernet
    lib/ioLibrary_Driver/Application/loopback
    
)

# PIOLib 정적 라이브러리 링크
target_link_libraries(qspi_test 
    ${PIOLIB_PATH}/build/libpio.a
    gpiod
)

# 컴파일 옵션
target_compile_options(qspi_test PRIVATE -Wall -Wextra -O0)

# 설치 설정
install(TARGETS qspi_test DESTINATION bin)
