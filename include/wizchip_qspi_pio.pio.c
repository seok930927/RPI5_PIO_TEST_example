#include "wizchip_qspi_pio.pio.h"

// PIO section symbols for linker
__attribute__((section(".piochips"))) const struct pio_program *pio_programs[] = {
    &wizchip_pio_spi_quad_write_read_program,
};