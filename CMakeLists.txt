cmake_minimum_required(VERSION 3.16)

project(qspi_pio)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

# PIOLib 경로 설정
set(PIOLIB_PATH "${CMAKE_CURRENT_SOURCE_DIR}/utils/piolib")

# 실행 파일 생성
add_executable(qspi_test src/qspi_test.c include/wizchip_qspi_pio.pio.c)
add_executable(qspi_test_clean src/qspi_test_clean.c)
add_executable(cnt_pio src/cnt_pio.c)

# PIOLib 헤더 포함
target_include_directories(qspi_test PRIVATE 
    ${PIOLIB_PATH}/include
    include
    ${CMAKE_CURRENT_SOURCE_DIR}/..
)

target_include_directories(qspi_test_clean PRIVATE 
    include
)

target_include_directories(cnt_pio PRIVATE 
    ${PIOLIB_PATH}/include
    include
)

# PIOLib 정적 라이브러리 링크
target_link_libraries(qspi_test 
    ${PIOLIB_PATH}/build/libpio.a
    gpiod
)

target_link_libraries(qspi_test_clean 
    gpiod
)

target_link_libraries(cnt_pio 
    ${PIOLIB_PATH}/build/libpio.a
)

# 컴파일 옵션
target_compile_options(qspi_test PRIVATE -Wall -Wextra)
target_compile_options(qspi_test_clean PRIVATE -Wall -Wextra)
target_compile_options(cnt_pio PRIVATE -Wall -Wextra)

# 설치 설정
install(TARGETS qspi_test cnt_pio DESTINATION bin)
