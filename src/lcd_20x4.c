#include "lcd_20x4.h"
#include "pico/stdlib.h"
#include <string.h>

static uint8_t backlight_val = LCD_BACKLIGHT_MASK;
static bool i2c_error_occurred = false;

// Helper estático para enviar dados brutos ao PCF8574 via I2C0 com proteção de timeout
static void lcd_write_raw(uint8_t val) {
    if (i2c_error_occurred) return;
    
    uint8_t data = val | backlight_val;
    // Timeout de 20 ms (20000 us) para evitar travar a CPU se o display não estiver presente
    int ret = i2c_write_timeout_us(LCD_I2C_INST, LCD_I2C_ADDR, &data, 1, false, 20000);
    if (ret < 0) {
        i2c_error_occurred = true;
    }
}

// Helper estático para gerar o pulso de Strobe (Enable)
static void lcd_strobe(uint8_t val) {
    lcd_write_raw(val | LCD_EN_MASK);
    sleep_us(2);
    lcd_write_raw(val & ~LCD_EN_MASK);
    sleep_us(50);
}

// Envia um único nibble de dados (4 bits superiores)
static void lcd_send_nibble(uint8_t val, uint8_t rs_flag) {
    uint8_t byte = (val & 0xF0) | rs_flag;
    lcd_write_raw(byte);
    lcd_strobe(byte);
}

// Envia um byte completo dividido em dois nibbles (high/low)
static void lcd_send_byte(uint8_t val, uint8_t rs_flag) {
    uint8_t high = val & 0xF0;
    uint8_t low = (val << 4) & 0xF0;
    lcd_send_nibble(high, rs_flag);
    lcd_send_nibble(low, rs_flag);
}

// Envia um comando de instrução
static void lcd_command(uint8_t cmd) {
    lcd_send_byte(cmd, 0);
}

// Envia um byte de dados
static void lcd_data(uint8_t data) {
    lcd_send_byte(data, LCD_RS_MASK);
}

bool lcd_init(void) {
    i2c_error_occurred = false;

    // 1. Inicializa o barramento I2C0
    i2c_init(LCD_I2C_INST, LCD_I2C_BAUDRATE);
    gpio_set_function(LCD_I2C_SDA_PIN, GPIO_FUNC_I2C);
    gpio_set_function(LCD_I2C_SCL_PIN, GPIO_FUNC_I2C);
    gpio_pull_up(LCD_I2C_SDA_PIN);
    gpio_pull_up(LCD_I2C_SCL_PIN);

    // 2. Aguarda a estabilização do LCD após ligar (>50ms)
    sleep_ms(60);

    // 3. Sequência de Reset recomendada pelo HD44780 (Modo 8-bits temporário)
    lcd_send_nibble(0x30, 0);
    sleep_ms(5);
    lcd_send_nibble(0x30, 0);
    sleep_us(150);
    lcd_send_nibble(0x30, 0);
    sleep_us(150);

    // 4. Configura para Modo 4-bits
    lcd_send_nibble(0x20, 0);
    sleep_us(150);

    // 5. Configurações Finais usando bytes completos
    lcd_command(LCD_FUNCTIONSET | LCD_4BITMODE | LCD_2LINE | LCD_5x8DOTS); // 0x28
    sleep_us(50);

    lcd_command(LCD_DISPLAYCONTROL | LCD_DISPLAYON | LCD_CURSOROFF | LCD_BLINKOFF); // 0x0C
    sleep_us(50);

    lcd_clear();

    lcd_command(LCD_ENTRYMODESET | LCD_ENTRYLEFT | LCD_ENTRYSHIFTDECREMENT); // 0x06
    sleep_us(50);

    return !i2c_error_occurred;
}

void lcd_clear(void) {
    lcd_command(LCD_CLEARDISPLAY);
    sleep_ms(3); // Comando limpar tela é lento (>1.53ms)
}

void lcd_set_cursor(int row, int col) {
    int row_offsets[] = { 0x00, 0x40, 0x14, 0x54 };
    if (row < 0) row = 0;
    if (row > 3) row = 3;
    if (col < 0) col = 0;
    if (col > 19) col = 19;
    lcd_command(LCD_SETDDRAMADDR | (row_offsets[row] + col));
}

void lcd_write_char(char c) {
    lcd_data((uint8_t)c);
}

void lcd_write_string(const char* s) {
    while (*s) {
        lcd_write_char(*s++);
    }
}

void lcd_write_line(int row, const char* s) {
    char buffer[21];
    int len = strlen(s);
    
    // Copia a string e preenche com espaços até completar 20 caracteres
    if (len >= 20) {
        strncpy(buffer, s, 20);
        buffer[20] = '\0';
    } else {
        strcpy(buffer, s);
        memset(buffer + len, ' ', 20 - len);
        buffer[20] = '\0';
    }
    
    lcd_set_cursor(row, 0);
    lcd_write_string(buffer);
}

void lcd_set_backlight(bool on) {
    backlight_val = on ? LCD_BACKLIGHT_MASK : 0x00;
    // Envia um byte vazio apenas para aplicar a mudança do backlight
    lcd_write_raw(0);
}

void lcd_create_char(uint8_t location, const uint8_t charmap[]) {
    location &= 0x7; // Só existem posições 0 a 7
    lcd_command(LCD_SETCGRAMADDR | (location << 3));
    for (int i = 0; i < 8; i++) {
        lcd_data(charmap[i]);
    }
}
