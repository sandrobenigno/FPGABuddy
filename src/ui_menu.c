#include "ui_menu.h"
#include "lcd_20x4.h"
#include <stdio.h>
#include <string.h>

static bool lcd_ok = false;

const char CATEGORIES[27] = {
    'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H', 'I',
    'J', 'K', 'L', 'M', 'N', 'O', 'P', 'Q', 'R',
    'S', 'T', 'U', 'V', 'W', 'X', 'Y', 'Z', '#'
};

static const char *SCANLINES_LABELS[] = {"Desl.", "25%", "50%", "75%"};
static const char *VOLUME_LABELS[] = {"Mudo", "33%", "66%", "100%"};
static const char *DIFFICULTY_LABELS[] = {"B (Facil)", "A (Dificil)"};
static const char *ASPECT_LABELS[] = {"4:3", "16:9"};
static const char *SWAP_LABELS[] = {"Nao", "Sim"};
static const char *VIDEO_LABELS[] = {"AUTO", "PAL", "NTSC"};

static MenuOption menu_options[MENU_MAX_ITEMS] = {
    {"Voltar ao Jogo", '\0', 0, 0, NULL},
    {"Reiniciar", '\0', 0, 0, NULL},
    {"Scanlines", 'S', 2, 3, SCANLINES_LABELS},
    {"Volume", 'A', 2, 3, VOLUME_LABELS},
    {"Dificuldade P1", 'X', 0, 1, DIFFICULTY_LABELS},
    {"Dificuldade P2", 'Y', 0, 1, DIFFICULTY_LABELS},
    {"Ajuste Tela", 'W', 1, 1, ASPECT_LABELS},
    {"Swap Joysticks", '&', 0, 1, SWAP_LABELS},
    {"Padrao Video", 'E', 0, 2, VIDEO_LABELS},
    {"De-comb", 'C', 0, 1, SWAP_LABELS},
    {"VBlank", 'M', 0, 1, SWAP_LABELS}
};

void ui_menu_init(bool is_lcd_ok) {
    lcd_ok = is_lcd_ok;
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

void ui_draw_grid_char(int index, bool show_visible, bool is_selected) {
    if (!lcd_ok) return;
    int row = 1 + (index / 9);
    int col = 1 + (index % 9) * 2;
    
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

void ui_draw_letters_grid_full(int selected_index) {
    if (!lcd_ok) return;
    lcd_set_cursor(1, 0);
    lcd_write_string(" A B C D E F G H I  ");
    lcd_set_cursor(2, 0);
    lcd_write_string(" J K L M N O P Q R  ");
    lcd_set_cursor(3, 0);
    lcd_write_string(" S T U V W X Y Z #  ");
    
    // Força a exibição da letra atualmente selecionada
    ui_draw_grid_char(selected_index, true, true);
}

void ui_draw_quick_settings_line(int row, int item_idx, bool is_focused, bool is_editing, bool blink_state) {
    if (!lcd_ok) return;
    if (item_idx >= MENU_MAX_ITEMS) {
        lcd_write_line(row, "");
        return;
    }
    
    char line_buf[32];
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
    lcd_write_line(row, line_buf);
}

void ui_draw_message(const char* l0, const char* l1, const char* l2, const char* l3) {
    if (!lcd_ok) return;
    lcd_clear();
    if(l0) lcd_write_line(0, l0);
    if(l1) lcd_write_line(1, l1);
    if(l2) lcd_write_line(2, l2);
    if(l3) lcd_write_line(3, l3);
}
