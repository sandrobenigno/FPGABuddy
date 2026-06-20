#ifndef CONFIG_H
#define CONFIG_H

#include "pico/stdlib.h"

// SPI Pins & Config
#define CS_FPGA_PIN        17
#define SD_CS_PIN          22
#define SPI_MISO_PIN       16
#define SPI_SCK_PIN        18
#define SPI_MOSI_PIN       19
#define SPI_FPGA_BAUDRATE  5000000

// Encoder Pins & Config
#define ENC_A_PIN          13
#define ENC_B_PIN          14
#define ENC_SW_PIN         15
#define ENC_VCC_PIN        12
#define DEBOUNCE_ENC_BTN_MS 40

// Auxiliary Controls
#define AUX_BUTTON_PIN     3
#define DEBOUNCE_AUX_BTN_MS 30
#define ONBOARD_LED_PIN    PICO_DEFAULT_LED_PIN

// LED Settings
#define LED_BRIGHTNESS_LIMIT 127

// RGB Colors for UI States
#define COLOR_RGB_SELECT_R 0
#define COLOR_RGB_SELECT_G 127
#define COLOR_RGB_SELECT_B 0

#define COLOR_RGB_PLAY_R   0
#define COLOR_RGB_PLAY_G   0
#define COLOR_RGB_PLAY_B   127

#define COLOR_RGB_CONFIG_R 127
#define COLOR_RGB_CONFIG_G 0
#define COLOR_RGB_CONFIG_B 0

#endif // CONFIG_H
