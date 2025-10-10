set(TARGET_NAME wizchip_iperf_lwip)
set(LWIP_PATH ${PICO_SDK_PATH}/lib/lwip)
set(TARGET_FILES    
    ${TARGET_NAME}.c
    ${LWIP_PATH}/src/apps/lwiperf/lwiperf.c
    )

add_executable(${TARGET_NAME} ${TARGET_FILES})

target_sources(${TARGET_NAME} PRIVATE
        ./../cJSON.c
        ./../iperf.c
        )

target_include_directories(${TARGET_NAME} PRIVATE
        ./../
        )

# This is required to include lwipopts.h
target_include_directories(pico_lwip INTERFACE        
        ${PORT_DIR}/lwip
        )

target_link_libraries(${TARGET_NAME} PRIVATE
        pico_stdlib
        hardware_spi
        hardware_dma
        ETHERNET_FILES
        IOLIBRARY_FILES
        LWIP_FILES
        )

pico_enable_stdio_usb(${TARGET_NAME} 1)
pico_enable_stdio_uart(${TARGET_NAME} 0)

pico_add_extra_outputs(${TARGET_NAME})
