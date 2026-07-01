#pragma once
#include <stdint.h>
#include <stdbool.h>
#include "db.h"

// Initialize hardware pins for SPI
void fpga_ctrl_init_pins(void);

void claim_spi_bus(void);
void release_spi_bus(void);
void restore_sd_spi(void);

void fpga_set_color_mode(bool color);
void fpga_send_config(char id, uint8_t value);
void fpga_send_controls_config(uint8_t mode);
void fpga_send_all_configs(void);

// Injects the ROM reading from SD to buffer and sending to FPGA via SPI
// Returns true on success, false on error.
bool fpga_inject_rom(GameInfo* game);

// Resets FPGA and ejects virtual cart
void fpga_reset_and_eject(void);

// Performs a quick reset of the FPGA core
void fpga_reset_core(void);

// Sets RGB LED color on FPGA (Target 0x00, CMD 0x02)
void fpga_set_rgb(uint8_t r, uint8_t g, uint8_t b);

// Sends key press/release event to FPGA (Target 0x01, CMD 0x01)
void fpga_send_key(uint8_t keycode, bool pressed);



// FPGA OSD Controls (Target 0x02)
void fpga_osd_set_visible(bool visible);
void fpga_osd_write_buffer(const uint8_t *buffer);




