cmake_minimum_required(VERSION 3.13)

# Importa o Pico SDK
include(pico_sdk_import.cmake)

project(fpgabuddy C CXX ASM)

# Habilita telemetria e diagnosticos detalhados da biblioteca carlk3
add_compile_definitions(USE_DBG_PRINTF=1 USE_PRINTF=1)

# Inicializa o Pico SDK
pico_sdk_init()

# Adiciona o subdiretorio da lib carlk3
add_subdirectory(no-OS-FatFS-SD-SDIO-SPI-RPi-Pico/src build-fat)

add_executable(fpgabuddy)

pico_generate_pio_header(fpgabuddy ${CMAKE_CURRENT_LIST_DIR}/pio/encoder.pio)

# Adiciona arquivos-fonte
target_sources(fpgabuddy PRIVATE
    src/main.c
    src/lcd_20x4.c
    src/hw_config.c
    src/db.c
    src/utils.c
    src/encoder.c
    src/fpga_ctrl.c
    src/ui_menu.c
    src/translation.c
    src/config_mgr.c
)

# Adiciona diretórios de include
target_include_directories(fpgabuddy PRIVATE
    ${CMAKE_CURRENT_LIST_DIR}/src
    ${CMAKE_CURRENT_BINARY_DIR} # Para encontrar os headers PIO autogerados
)

# Linka com as bibliotecas do Pico SDK e carlk3
target_link_libraries(fpgabuddy PRIVATE
    pico_stdlib
    hardware_i2c
    hardware_spi
    hardware_pio
    no-OS-FatFS-SD-SDIO-SPI-RPi-Pico
)

# HABILITA SAÍDA DE DEBUG VIA PORTA USB (VIRTUAL COM PORT) E UART
pico_enable_stdio_usb(fpgabuddy 1)
pico_enable_stdio_uart(fpgabuddy 1)

# Habilita saídas adicionais (como binário .uf2, .hex, etc)
pico_add_extra_outputs(fpgabuddy)
