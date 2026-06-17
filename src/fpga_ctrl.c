#include "fpga_ctrl.h"
#include "pico/stdlib.h"
#include "hardware/spi.h"
#include "hardware/dma.h"
#include "hw_config.h"
#include "ff.h"
#include "ui_menu.h"
#include <stdio.h>
#include <string.h>

#define CS_FPGA_PIN    17
#define SD_CS_PIN      22

static uint8_t rom_buffer[64 * 1024];

void fpga_ctrl_init_pins(void) {
    gpio_init(CS_FPGA_PIN);
    gpio_set_dir(CS_FPGA_PIN, GPIO_OUT);
    gpio_put(CS_FPGA_PIN, 1);
    gpio_set_drive_strength(CS_FPGA_PIN, GPIO_DRIVE_STRENGTH_12MA);

    gpio_init(SD_CS_PIN);
    gpio_set_dir(SD_CS_PIN, GPIO_OUT);
    gpio_put(SD_CS_PIN, 1);
    gpio_set_drive_strength(SD_CS_PIN, GPIO_DRIVE_STRENGTH_4MA);
}

void release_spi_bus(void) {
    spi_deinit(spi0);
    
    const uint pins[] = {16, 18, 19, 22};
    for (size_t i = 0; i < sizeof(pins)/sizeof(pins[0]); i++) {
        gpio_init(pins[i]);
        gpio_set_dir(pins[i], GPIO_IN);
        gpio_pull_up(pins[i]);
    }
    
    gpio_init(CS_FPGA_PIN);
    gpio_set_dir(CS_FPGA_PIN, GPIO_OUT);
    gpio_put(CS_FPGA_PIN, 1);
    gpio_set_drive_strength(CS_FPGA_PIN, GPIO_DRIVE_STRENGTH_12MA);
}

void claim_spi_bus(void) {
    spi_init(spi0, 5000000);
    spi_set_format(spi0, 8, SPI_CPOL_0, SPI_CPHA_1, SPI_MSB_FIRST);
    
    gpio_set_function(16, GPIO_FUNC_SPI);
    gpio_set_function(18, GPIO_FUNC_SPI);
    gpio_set_function(19, GPIO_FUNC_SPI);
    
    gpio_init(CS_FPGA_PIN);
    gpio_set_dir(CS_FPGA_PIN, GPIO_OUT);
    gpio_put(CS_FPGA_PIN, 1);
    gpio_set_drive_strength(CS_FPGA_PIN, GPIO_DRIVE_STRENGTH_12MA);
    
    gpio_init(SD_CS_PIN);
    gpio_set_dir(SD_CS_PIN, GPIO_OUT);
    gpio_put(SD_CS_PIN, 1);
    gpio_set_drive_strength(SD_CS_PIN, GPIO_DRIVE_STRENGTH_4MA);
}

void fpga_send_config(char id, uint8_t value) {
    claim_spi_bus();
    
    gpio_put(CS_FPGA_PIN, 0);
    sleep_us(5);
    uint8_t cmd[4] = {
        0x00, // Target 0 (SYS)
        0x04, // CMD 4 (SETVAL)
        id,
        value
    };
    spi_write_blocking(spi0, cmd, 4);
    sleep_us(5);
    gpio_put(CS_FPGA_PIN, 1);
    sleep_us(10);
    
    // Restaura o formato SPI para o modo esperado pelo cartão SD (SPI Mode 0)
    spi_set_format(spi0, 8, SPI_CPOL_0, SPI_CPHA_0, SPI_MSB_FIRST);
}

void fpga_set_color_mode(bool color) {
    fpga_send_config('O', color ? 0x01 : 0x00);
}

void restore_sd_spi(void) {
    sd_card_t *sd_card_p = sd_get_by_num(0);
    if (sd_card_p && sd_card_p->type == SD_IF_SPI && sd_card_p->spi_if_p && sd_card_p->spi_if_p->spi) {
        spi_t *spi_p = sd_card_p->spi_if_p->spi;
        if (spi_p->initialized) {
            if (!spi_p->use_static_dma_channels) {
                dma_channel_unclaim(spi_p->tx_dma);
                dma_channel_unclaim(spi_p->rx_dma);
            }
            spi_p->initialized = false;
        }
        my_spi_init(spi_p);
        sd_card_p->state.m_Status |= STA_NOINIT;
    }
}

void fpga_send_all_configs(void) {
    fpga_set_color_mode(true);
    
    for (int i = 1; i < MENU_MAX_ITEMS; i++) {
        MenuOption* opt = ui_get_menu_option(i);
        if(!opt) continue;
        fpga_send_config(opt->id, opt->value);
    }
}

bool fpga_inject_rom(GameInfo* game) {
    ui_draw_message("LENDO DO SD...      ", "====================", game->nome, "Aguarde...          ");

    char filepath[80];
    snprintf(filepath, sizeof(filepath), "/roms/%s.bin", game->md5);
    FIL file;
    FRESULT fr = f_open(&file, filepath, FA_READ);
    if (fr != FR_OK) {
        snprintf(filepath, sizeof(filepath), "/%s.bin", game->md5);
        fr = f_open(&file, filepath, FA_READ);
    }
    
    if (fr != FR_OK) {
        printf("[ERRO] Arquivo ROM '%s' nao encontrado no SD.\n", filepath);
        ui_draw_message("  ROM NAO ENCONTR.  ", "====================", "Verifique o SD Card ", "Voltando...         ");
        sleep_ms(2000);
        return false;
    }
    
    FSIZE_t file_size = file.obj.objsize;
    if (file_size > sizeof(rom_buffer)) {
        printf("[ERRO] Arquivo ROM muito grande (%d bytes). Limite do buffer eh %d bytes.\n", (int)file_size, (int)sizeof(rom_buffer));
        f_close(&file);
        ui_draw_message("  ROM MUITO GRANDE  ", "====================", "Tamanho excede buffer", "Voltando...         ");
        sleep_ms(2000);
        return false;
    }
    
    UINT bytes_read = 0;
    fr = f_read(&file, rom_buffer, file_size, &bytes_read);
    f_close(&file);
    
    if (fr != FR_OK || bytes_read != file_size) {
        printf("[ERRO] Falha ao ler a ROM para o buffer. fr=%d, lidos=%d\n", fr, (int)bytes_read);
        ui_draw_message("  ERRO DE LEITURA   ", "====================", "Erro ao ler arquivo ", "Voltando...         ");
        sleep_ms(2000);
        return false;
    }
    
    printf("[SD] ROM lida com sucesso: %d bytes carregados na RAM do Pico.\n", (int)file_size);
    
    ui_draw_message("INJETANDO ROM...    ", "====================", game->nome, "Transmitindo SPI... ");
    
    claim_spi_bus();
    gpio_put(SD_CS_PIN, 1);
    sleep_us(10);
    
    printf("[FPGA] Enviando tamanho da ROM (%d bytes)...\n", (int)file_size);
    gpio_put(CS_FPGA_PIN, 0);
    sleep_us(5);
    uint8_t cmd_inserted[7] = {
        0x03, 0x04, 0x00,
        (file_size >> 24) & 0xFF,
        (file_size >> 16) & 0xFF,
        (file_size >> 8) & 0xFF,
        file_size & 0xFF
    };
    spi_write_blocking(spi0, cmd_inserted, 7);
    sleep_us(5);
    gpio_put(CS_FPGA_PIN, 1);
    sleep_us(10);
    
    printf("[FPGA] Iniciando injeção da ROM na RAM...\n");
    gpio_put(CS_FPGA_PIN, 0);
    sleep_us(5);
    uint8_t stream_header[2] = {0x03, 0x08};
    spi_write_blocking(spi0, stream_header, 2);
    spi_write_blocking(spi0, rom_buffer, file_size);
    sleep_us(5);
    gpio_put(CS_FPGA_PIN, 1);
    sleep_us(10);
    
    // Reseta a opção "Padrao Video" (ID 'E') para 0 (AUTO) para o próximo jogo
    for (int i = 1; i < MENU_MAX_ITEMS; i++) {
        MenuOption* opt = ui_get_menu_option(i);
        if (opt && opt->id == 'E') {
            opt->value = 0; // AUTO
            break;
        }
    }

    fpga_send_all_configs();
    printf("[FPGA] Injeção concluída e console ativo!\n");
    return true;
}

void fpga_reset_and_eject(void) {
    claim_spi_bus();
    
    printf("[FPGA] Ejetando cartucho virtual e resetando a FPGA...\n");
    spi_set_baudrate(spi0, 5000000); // 5 MHz
    
    gpio_put(CS_FPGA_PIN, 0);
    sleep_us(5);
    uint8_t reset_cmd[4] = {0x00, 0x04, 'R', 0x03};
    spi_write_blocking(spi0, reset_cmd, 4);
    sleep_us(5);
    gpio_put(CS_FPGA_PIN, 1);
    sleep_us(10);
    
    gpio_put(CS_FPGA_PIN, 0);
    sleep_us(5);
    uint8_t cmd_eject[7] = {0x03, 0x04, 0x00, 0x00, 0x00, 0x00, 0x00};
    spi_write_blocking(spi0, cmd_eject, 7);
    sleep_us(5);
    gpio_put(CS_FPGA_PIN, 1);
    sleep_us(10);
    
    gpio_put(CS_FPGA_PIN, 0);
    sleep_us(5);
    uint8_t cmd_direct_0[7] = {0x03, 0x06, 0x00, 0x00, 0x00, 0x00, 0x00};
    spi_write_blocking(spi0, cmd_direct_0, 7);
    sleep_us(5);
    gpio_put(CS_FPGA_PIN, 1);
    sleep_us(10);
    
    gpio_put(CS_FPGA_PIN, 0);
    sleep_us(5);
    uint8_t run_cmd[4] = {0x00, 0x04, 'R', 0x00};
    spi_write_blocking(spi0, run_cmd, 4);
    sleep_us(5);
    gpio_put(CS_FPGA_PIN, 1);
    
    fpga_send_all_configs();
    printf("[FPGA] Reset e Ejection concluidos.\n");
    sleep_ms(10);
}

void fpga_set_rgb(uint8_t r, uint8_t g, uint8_t b) {
    claim_spi_bus();
    
    gpio_put(CS_FPGA_PIN, 0);
    sleep_us(5);
    uint8_t cmd[5] = {0x00, 0x02, r, g, b};
    spi_write_blocking(spi0, cmd, 5);
    sleep_us(5);
    gpio_put(CS_FPGA_PIN, 1);
    sleep_us(10);
    
    // Restaura o formato SPI para o modo esperado pelo cartão SD (SPI Mode 0)
    spi_set_format(spi0, 8, SPI_CPOL_0, SPI_CPHA_0, SPI_MSB_FIRST);
}

void fpga_send_key(uint8_t keycode, bool pressed) {
    claim_spi_bus();
    
    gpio_put(CS_FPGA_PIN, 0);
    sleep_us(5);
    uint8_t cmd[3] = {
        0x01, // Target 1 (HID)
        0x01, // Cmd 1 (SPI_HID_KEYBOARD)
        pressed ? (keycode & 0x7F) : (keycode | 0x80)
    };
    spi_write_blocking(spi0, cmd, 3);
    sleep_us(5);
    gpio_put(CS_FPGA_PIN, 1);
    sleep_us(10);
    
    // Restaura o formato SPI para o modo esperado pelo cartão SD (SPI Mode 0)
    spi_set_format(spi0, 8, SPI_CPOL_0, SPI_CPHA_0, SPI_MSB_FIRST);
}

void fpga_send_mouse(uint8_t buttons, int8_t dx, int8_t dy) {
    claim_spi_bus();
    
    gpio_put(CS_FPGA_PIN, 0);
    sleep_us(5);
    uint8_t cmd[5] = {
        0x01, // Target 1 (HID)
        0x02, // Cmd 2 (SPI_HID_MOUSE)
        buttons,
        (uint8_t)dx,
        (uint8_t)dy
    };
    spi_write_blocking(spi0, cmd, 5);
    sleep_us(5);
    gpio_put(CS_FPGA_PIN, 1);
    sleep_us(10);
    
    // Restaura o formato SPI para o modo esperado pelo cartão SD (SPI Mode 0)
    spi_set_format(spi0, 8, SPI_CPOL_0, SPI_CPHA_0, SPI_MSB_FIRST);
}



