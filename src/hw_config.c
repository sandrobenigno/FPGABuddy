#include "hw_config.h"

/* Configuration of hardware SPI object */
static spi_t spi = {
    .hw_inst = spi0,  // SPI component
    .sck_gpio = 18,    // GPIO number
    .mosi_gpio = 19,
    .miso_gpio = 16,
    .baud_rate = 8 * 1000 * 1000, // 8 MHz (stable speed with high drive strength)
    .set_drive_strength = true,
    .mosi_gpio_drive_strength = GPIO_DRIVE_STRENGTH_12MA,
    .sck_gpio_drive_strength = GPIO_DRIVE_STRENGTH_12MA
};

/* SPI Interface */
static sd_spi_if_t spi_if = {
    .spi = &spi,  // Pointer to the SPI driving this card
    .ss_gpio = 22, // The SPI slave select GPIO for this SD card
    .set_drive_strength = true,
    .ss_gpio_drive_strength = GPIO_DRIVE_STRENGTH_12MA
};

/* Configuration of the SD Card socket object */
static sd_card_t sd_card = {
    .type = SD_IF_SPI,
    .spi_if_p = &spi_if  // Pointer to the SPI interface driving this card
};

/* ********************************************************************** */

size_t sd_get_num() { return 1; }

/**
 * @brief Get a pointer to an SD card object by its number.
 */
sd_card_t *sd_get_by_num(size_t num) {
    if (0 == num) {
        return &sd_card;
    } else {
        return NULL;
    }
}
