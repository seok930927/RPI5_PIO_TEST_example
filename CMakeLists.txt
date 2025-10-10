cmake_minimum_required(VERSION 3.16)

project(qspi_pio)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

# PIOLib 경로 설정
set(PIOLIB_PATH "${CMAKE_CURRENT_SOURCE_DIR}/utils/piolib")

# 공통 소스 파일들
file(GLOB IO_LIB_SRC lib/ioLibrary_Driver/Ethernet/*.c)
file(GLOB W6300_SRC lib/ioLibrary_Driver/Ethernet/W6300/*.c)
file(GLOB LIB_loopback lib/ioLibrary_Driver/Application/loopback/*.c)

# 공통 소스들을 변수로 정의
set(COMMON_SOURCES
    src/pio_func.c 
    src/wizchip_spi.c
    inc/wizchip_qspi_pio.pio.c 
    ${IO_LIB_SRC} 
    ${W6300_SRC}
)

# 공통 include 디렉토리
set(COMMON_INCLUDES
    ${PIOLIB_PATH}/inc
    inc
    src
    lib/ioLibrary_Driver/
    lib/ioLibrary_Driver/Ethernet
    lib/ioLibrary_Driver/Application/loopback
)


# iperf 소스코드 변수로 정의
set(IPERF_TEST_SRC
    src/iperf3/cJSON.c  
    src/iperf3/iperf.c
)

# 공통 include 디렉토리
set(IPERF_TEST_INC
    src/iperf3/
)


# 공통 라이브러리
set(COMMON_LIBRARIES
    pio
    gpiod
)

# ===== 애플리케이션 1: loopback =====
add_executable(loopback 
    src/loopback_test.c 
    ${COMMON_SOURCES}
    ${LIB_loopback}
)

target_include_directories(loopback PRIVATE ${COMMON_INCLUDES})
target_link_libraries(loopback ${COMMON_LIBRARIES})
add_dependencies(loopback pio)
target_compile_options(loopback PRIVATE -Wall -Wextra -O0)




# ===== 애플리케이션 2: qspi_test =====
add_executable(iperf
    src/iperf3/toe/wizchip_iperf_toe.c 
    ${COMMON_SOURCES}
    ${LIB_loopback}
    ${IPERF_TEST_SRC}

)

target_include_directories(iperf PRIVATE ${COMMON_INCLUDES} ${IPERF_TEST_INC})
target_link_libraries(iperf ${COMMON_LIBRARIES} )
add_dependencies(iperf pio)
target_compile_options(iperf PRIVATE -Wall -Wextra -Ofast)




# piolib만 빌드 (다른 utils는 추가 의존성 필요)
add_subdirectory(utils/piolib)

# 설치 설정
install(TARGETS loopback iperf  DESTINATION bin)
