#pragma once
#include <stdint.h>
#include <stdbool.h>

#define MENU_MAX_ITEMS 12

typedef struct {
    const char *label;
    char id;
    uint8_t value;
    uint8_t max_val;
    const char **value_labels;
} MenuOption;

extern const char CATEGORIES[27];

void ui_menu_init(bool is_lcd_ok);
bool ui_is_lcd_ok(void);

void ui_draw_grid_char(int index, bool show_visible, bool is_selected);
void ui_draw_letters_grid_full(int selected_index);
void ui_draw_quick_settings_line(int row, int item_idx, bool is_focused, bool is_editing, bool blink_state);
void ui_draw_message(const char* l0, const char* l1, const char* l2, const char* l3);
void ui_write_game_line(const char* game_name);

MenuOption* ui_get_menu_option(int index);

