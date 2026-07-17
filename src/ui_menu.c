#include "ui_menu.h"
#include "lcd_20x4.h"
#include "font_5x8.h"
#include "fpga_ctrl.h"
#include "translation.h"
#include <stdio.h>
#include <string.h>

static bool lcd_ok = false;

// Buffer de tela virtual para simulação do LCD 20x4
static char virtual_screen[4][21];
static int virtual_cursor_row = 0;
static int virtual_cursor_col = 0;

const char CATEGORIES[27] = {
    'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H', 'I',
    'J', 'K', 'L', 'M', 'N', 'O', 'P', 'Q', 'R',
    'S', 'T', 'U', 'V', 'W', 'X', 'Y', 'Z', '#'
};

static const char *SCANLINES_LABELS[4];
static const char *VOLUME_LABELS[4];
static const char *DIFFICULTY_LABELS[2];
static const char *ASPECT_LABELS[2];
static const char *SWAP_LABELS[2];
static const char *VIDEO_LABELS[3];
static const char *CONTROLS_LABELS[2];
static const char *VSYNC_STAB_LABELS[3];

static MenuOption menu_options[MENU_MAX_ITEMS] = {
    {NULL, '\0', 0, 0, NULL},
    {NULL, '\0', 0, 0, NULL},
    {NULL, '\0', 0, 1, CONTROLS_LABELS},
    {NULL, 'S', 2, 3, SCANLINES_LABELS},
    {NULL, 'C', 0, 1, SWAP_LABELS},
    {NULL, 'A', 2, 3, VOLUME_LABELS},
    {NULL, 'X', 0, 1, DIFFICULTY_LABELS},
    {NULL, 'Y', 0, 1, DIFFICULTY_LABELS},
    {NULL, 'W', 1, 1, ASPECT_LABELS},
    {NULL, '&', 0, 1, SWAP_LABELS},
    {NULL, 'E', 0, 2, VIDEO_LABELS},
    {NULL, 'M', 0, 1, SWAP_LABELS},
    {NULL, 'T', 0, 2, VSYNC_STAB_LABELS}
};

// Funções utilitárias do LCD virtual
static void virtual_lcd_clear(void) {
    for (int r = 0; r < 4; r++) {
        for (int c = 0; c < 20; c++) {
            virtual_screen[r][c] = ' ';
        }
        virtual_screen[r][20] = '\0';
    }
    virtual_cursor_row = 0;
    virtual_cursor_col = 0;
}

static void virtual_lcd_set_cursor(int row, int col) {
    if (row >= 0 && row < 4) virtual_cursor_row = row;
    if (col >= 0 && col < 20) virtual_cursor_col = col;
}

static void virtual_lcd_write_char(char c) {
    if (virtual_cursor_row >= 0 && virtual_cursor_row < 4 &&
        virtual_cursor_col >= 0 && virtual_cursor_col < 20) {
        virtual_screen[virtual_cursor_row][virtual_cursor_col] = c;
        virtual_cursor_col++;
        if (virtual_cursor_col >= 20) {
            virtual_cursor_col = 0;
            virtual_cursor_row++;
            if (virtual_cursor_row >= 4) {
                virtual_cursor_row = 0;
            }
        }
    }
}

static void virtual_lcd_write_string(const char *str) {
    while (*str) {
        virtual_lcd_write_char(*str++);
    }
}

static void virtual_lcd_write_line(int row, const char *str) {
    if (row < 0 || row >= 4) return;
    
    int len = 0;
    if (str) {
        len = strlen(str);
        if (len > 20) len = 20;
        memcpy(virtual_screen[row], str, len);
    }
    
    for (int c = len; c < 20; c++) {
        virtual_screen[row][c] = ' ';
    }
    virtual_screen[row][20] = '\0';
}

static void osd_draw_byte(uint8_t *buf, int x, int y, uint8_t b) {
    if (x < 0 || x >= 128) return;
    int page_a = y / 8;
    int shift_a = y % 8;
    
    if (page_a >= 0 && page_a < 8) {
        buf[page_a * 128 + x] |= (b << shift_a);
    }
    
    int page_b = page_a + 1;
    if (shift_a > 0 && page_b >= 0 && page_b < 8) {
        buf[page_b * 128 + x] |= (b >> (8 - shift_a));
    }
}

static void virtual_lcd_flush(void) {
    static uint8_t osd_buf[1024];
    memset(osd_buf, 0x00, 1024);
    
    for (int r = 0; r < 4; r++) {
        int y_start = 13 + r * 10; // Centralizado verticalmente com 2px de gap entre as linhas de 8px
        for (int c = 0; c < 20; c++) {
            char val = virtual_screen[r][c];
            int idx = 0;
            if (val >= 32 && val <= 126) {
                idx = val - 32;
            }
            
            // Inicia na coluna 4 para centralizar horizontalmente (120 pixels ativos em 128)
            int start_col = 4 + c * 6;
            
            for (int font_col = 0; font_col < 5; font_col++) {
                int col_idx = start_col + font_col;
                osd_draw_byte(osd_buf, col_idx, y_start, font_5x8[idx][font_col]);
            }
            
            int col_idx = start_col + 5; // A 6ª coluna é o espaço/gap
            osd_draw_byte(osd_buf, col_idx, y_start, 0x00);
        }
    }
    
    fpga_osd_write_buffer(osd_buf);
}

void ui_menu_init(bool is_lcd_ok) {
    lcd_ok = is_lcd_ok;
    virtual_lcd_clear();
    ui_menu_update_labels();
}

bool ui_is_lcd_ok(void) {
    return lcd_ok;
}

MenuOption* ui_get_menu_option(int index) {
    if (index >= 0 && index < MENU_MAX_ITEMS) {
        return &menu_options[index];
    }
    return NULL;
}

void ui_menu_update_labels(void) {
    SCANLINES_LABELS[0] = t(MSG_OFF);
    SCANLINES_LABELS[1] = "25%";
    SCANLINES_LABELS[2] = "50%";
    SCANLINES_LABELS[3] = "75%";
    
    VOLUME_LABELS[0] = t(MSG_MUTE);
    VOLUME_LABELS[1] = "33%";
    VOLUME_LABELS[2] = "66%";
    VOLUME_LABELS[3] = "100%";
    
    DIFFICULTY_LABELS[0] = t(MSG_EASY);
    DIFFICULTY_LABELS[1] = t(MSG_HARD);
    
    ASPECT_LABELS[0] = "4:3";
    ASPECT_LABELS[1] = "16:9";
    
    SWAP_LABELS[0] = t(MSG_NO);
    SWAP_LABELS[1] = t(MSG_YES);
    
    VIDEO_LABELS[0] = t(MSG_AUTO);
    VIDEO_LABELS[1] = "PAL";
    VIDEO_LABELS[2] = "NTSC";
    
    CONTROLS_LABELS[0] = "JOY";
    CONTROLS_LABELS[1] = "PAD";
    
    VSYNC_STAB_LABELS[0] = t(MSG_SMART);
    VSYNC_STAB_LABELS[1] = t(MSG_FIXED);
    VSYNC_STAB_LABELS[2] = t(MSG_NONE);
    
    menu_options[0].label = t(MSG_MENU_BACK_TO_GAME);
    menu_options[1].label = t(MSG_MENU_RESTART);
    menu_options[2].label = t(MSG_MENU_CONTROLS);
    menu_options[3].label = t(MSG_MENU_SCANLINES);
    menu_options[4].label = t(MSG_MENU_DECOMB);
    menu_options[5].label = t(MSG_MENU_VOLUME);
    menu_options[6].label = t(MSG_MENU_DIFF_P1);
    menu_options[7].label = t(MSG_MENU_DIFF_P2);
    menu_options[8].label = t(MSG_MENU_ASPECT);
    menu_options[9].label = t(MSG_MENU_SWAP_JOYS);
    menu_options[10].label = t(MSG_MENU_VIDEO_STD);
    menu_options[11].label = t(MSG_MENU_VBLANK);
    menu_options[12].label = t(MSG_MENU_VSYNC_STAB);
}

void ui_draw_grid_char(int index, bool show_visible, bool is_selected) {
    int row = 1 + (index / 9);
    int col = 1 + (index % 9) * 2;
    
    if (lcd_ok) {
        if (is_selected) {
            lcd_set_cursor(row, col - 1);
            lcd_write_char('[');
            lcd_set_cursor(row, col + 1);
            lcd_write_char(']');
            lcd_set_cursor(row, col);
            lcd_write_char(show_visible ? CATEGORIES[index] : ' ');
        } else {
            lcd_set_cursor(row, col - 1);
            lcd_write_char(' ');
            lcd_set_cursor(row, col + 1);
            lcd_write_char(' ');
            lcd_set_cursor(row, col);
            lcd_write_char(CATEGORIES[index]);
        }
    }
    
    // Atualiza buffer virtual (OSD)
    if (is_selected) {
        virtual_lcd_set_cursor(row, col - 1);
        virtual_lcd_write_char('[');
        virtual_lcd_set_cursor(row, col + 1);
        virtual_lcd_write_char(']');
        virtual_lcd_set_cursor(row, col);
        virtual_lcd_write_char(show_visible ? CATEGORIES[index] : ' ');
    } else {
        virtual_lcd_set_cursor(row, col - 1);
        virtual_lcd_write_char(' ');
        virtual_lcd_set_cursor(row, col + 1);
        virtual_lcd_write_char(' ');
        virtual_lcd_set_cursor(row, col);
        virtual_lcd_write_char(CATEGORIES[index]);
    }
    virtual_lcd_flush();
}

void ui_draw_letters_grid_full(int selected_index) {
    if (lcd_ok) {
        lcd_set_cursor(1, 0);
        lcd_write_string(" A B C D E F G H I  ");
        lcd_set_cursor(2, 0);
        lcd_write_string(" J K L M N O P Q R  ");
        lcd_set_cursor(3, 0);
        lcd_write_string(" S T U V W X Y Z #  ");
    }
    
    // Desenha no buffer virtual (OSD)
    virtual_lcd_set_cursor(1, 0);
    virtual_lcd_write_string(" A B C D E F G H I  ");
    virtual_lcd_set_cursor(2, 0);
    virtual_lcd_write_string(" J K L M N O P Q R  ");
    virtual_lcd_set_cursor(3, 0);
    virtual_lcd_write_string(" S T U V W X Y Z #  ");
    
    ui_draw_grid_char(selected_index, true, true);
}

void ui_draw_quick_settings_line(int row, int item_idx, bool is_focused, bool is_editing, bool blink_state) {
    char line_buf[32];
    if (item_idx >= MENU_MAX_ITEMS) {
        strcpy(line_buf, "");
    } else {
        MenuOption opt = menu_options[item_idx];
        if (item_idx == 0 || item_idx == 1) {
            snprintf(line_buf, sizeof(line_buf), "%s %s", is_focused ? ">" : " ", opt.label);
        } else {
            const char *val_str = opt.value_labels[opt.value];
            if (is_focused && is_editing) {
                if (blink_state) {
                    snprintf(line_buf, sizeof(line_buf), "> %s:[%s]", opt.label, val_str);
                } else {
                    int val_len = strlen(val_str);
                    char spaces[16];
                    if (val_len > 15) val_len = 15;
                    memset(spaces, ' ', val_len);
                    spaces[val_len] = '\0';
                    snprintf(line_buf, sizeof(line_buf), "> %s:[%s]", opt.label, spaces);
                }
            } else {
                snprintf(line_buf, sizeof(line_buf), "%s %s: %s", is_focused ? ">" : " ", opt.label, val_str);
            }
        }
    }
    
    if (lcd_ok) {
        lcd_write_line(row, line_buf);
    }
    
    // Atualiza buffer virtual (OSD)
    virtual_lcd_write_line(row, line_buf);
    virtual_lcd_flush();
}

void ui_draw_message(const char* l0, const char* l1, const char* l2, const char* l3) {
    if (lcd_ok) {
        lcd_clear();
        if(l0) lcd_write_line(0, l0);
        if(l1) lcd_write_line(1, l1);
        if(l2) lcd_write_line(2, l2);
        if(l3) lcd_write_line(3, l3);
    }
    
    // Atualiza buffer virtual (OSD)
    virtual_lcd_clear();
    if(l0) virtual_lcd_write_line(0, l0);
    if(l1) virtual_lcd_write_line(1, l1);
    if(l2) virtual_lcd_write_line(2, l2);
    if(l3) virtual_lcd_write_line(3, l3);
    virtual_lcd_flush();
}

void ui_write_game_line(const char* game_name) {
    char line1[21];
    char line2[21];
    
    int len = strlen(game_name);
    if (len <= 20) {
        snprintf(line1, sizeof(line1), "%s", game_name);
        snprintf(line2, sizeof(line2), "%s", t(MSG_NAV_INSTRUCTION));
    } else {
        // Encontra o último caractere de espaço dentro dos primeiros 20 caracteres
        int split_idx = 20;
        for (int i = 20; i >= 0; i--) {
            if (game_name[i] == ' ') {
                split_idx = i;
                break;
            }
        }
        
        // Copia a primeira parte
        strncpy(line1, game_name, split_idx);
        line1[split_idx] = '\0';
        
        // Pula o espaço se houver
        int start2 = (game_name[split_idx] == ' ') ? split_idx + 1 : split_idx;
        
        // Copia a segunda parte (limita a 20 caracteres)
        strncpy(line2, game_name + start2, 20);
        line2[20] = '\0';
    }
    
    if (lcd_ok) {
        lcd_write_line(2, line1);
        lcd_write_line(3, line2);
    }
    
    // Atualiza buffer virtual (OSD)
    virtual_lcd_write_line(2, line1);
    virtual_lcd_write_line(3, line2);
    virtual_lcd_flush();
}
