#ifndef LCD_20X4_H
#define LCD_20X4_H

#include <stdint.h>
#include <stdbool.h>
#include "hardware/i2c.h"

// Endereço I2C padrão do expansor PCF8574 (geralmente 0x27 ou 0x3F)
#define LCD_I2C_ADDR          0x27

// Pinos GPIO do RP2040 associados ao I2C0
#define LCD_I2C_SDA_PIN       20
#define LCD_I2C_SCL_PIN       21
#define LCD_I2C_INST          i2c0
#define LCD_I2C_BAUDRATE      100000 // 100 kHz (Standard Mode - mais ágil e compatível)

// Mapeamento de pinos do PCF8574 para a tela HD44780
#define LCD_RS_MASK           0x01 // P0 - Register Select (0=Cmd, 1=Data)
#define LCD_RW_MASK           0x02 // P1 - Read/Write (geralmente 0/GND)
#define LCD_EN_MASK           0x04 // P2 - Enable (Pulse)
#define LCD_BACKLIGHT_MASK    0x08 // P3 - Controle de Backlight (1=On, 0=Off)
#define LCD_D4_MASK           0x10 // P4 - Data line 4
#define LCD_D5_MASK           0x20 // P5 - Data line 5
#define LCD_D6_MASK           0x40 // P6 - Data line 6
#define LCD_D7_MASK           0x80 // P7 - Data line 7

// Comandos HD44780 do LCD
#define LCD_CLEARDISPLAY      0x01
#define LCD_RETURNHOME        0x02
#define LCD_ENTRYMODESET      0x04
#define LCD_DISPLAYCONTROL    0x08
#define LCD_CURSORSHIFT       0x10
#define LCD_FUNCTIONSET       0x20
#define LCD_SETCGRAMADDR      0x40
#define LCD_SETDDRAMADDR      0x80

// Flags para Display Control
#define LCD_DISPLAYON         0x04
#define LCD_DISPLAYOFF        0x00
#define LCD_CURSORON          0x02
#define LCD_CURSOROFF         0x00
#define LCD_BLINKON           0x01
#define LCD_BLINKOFF          0x00

// Flags para Entry Mode Set
#define LCD_ENTRYLEFT         0x02
#define LCD_ENTRYSHIFTINCREMENT 0x01
#define LCD_ENTRYSHIFTDECREMENT 0x00

// Flags para Function Set (Modo 4-bits, 2 linhas, fonte 5x8)
#define LCD_4BITMODE          0x00
#define LCD_2LINE             0x08
#define LCD_5x8DOTS           0x00

/**
 * @brief Inicializa o barramento I2C0 e a tela LCD 20x4 em modo 4 bits.
 * @return true se o expansor respondeu no barramento, false se houve timeout.
 */
bool lcd_init(void);

/**
 * @brief Limpa toda a tela LCD e retorna o cursor para (0,0).
 */
void lcd_clear(void);

/**
 * @brief Move o cursor para a linha e coluna especificadas.
 * @param row Linha (0 a 3)
 * @param col Coluna (0 a 19)
 */
void lcd_set_cursor(int row, int col);

/**
 * @brief Escreve um único caractere no display.
 * @param c Caractere a ser escrito
 */
void lcd_write_char(char c);

/**
 * @brief Escreve uma string no display na posição atual do cursor.
 * @param s String nula-terminada
 */
void lcd_write_string(const char* s);

/**
 * @brief Escreve uma string em uma linha específica, limpando e completando a linha com espaços.
 * @param row Linha (0 a 3)
 * @param s String a ser escrita (máximo 20 caracteres)
 */
void lcd_write_line(int row, const char* s);

/**
 * @brief Controla o estado do Backlight da tela.
 * @param on true para ligado, false para desligado
 */
void lcd_set_backlight(bool on);

/**
 * @brief Cria um caractere customizado na CGRAM do LCD.
 * @param location ID do caractere customizado (0 a 7)
 * @param charmap Array de 8 bytes com o padrão de pixels (5x8 dots)
 */
void lcd_create_char(uint8_t location, const uint8_t charmap[]);

#endif // LCD_20X4_H
