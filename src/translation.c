#include "translation.h"
#include <stddef.h>

static Language current_lang = LANG_PT_BR;

static const char* const strings_pt[MSG_COUNT] = {
    [MSG_DB_MENU_TITLE]        = "** GERENCIAR BANCO **",
    [MSG_DB_MENU_UPDATE]       = "Atualizar Jogos",
    [MSG_DB_MENU_LANG]         = "Idioma: ",
    [MSG_DB_MENU_BACK]         = "Voltar",
    [MSG_READING_SD]           = "LENDO DO SD...      ",
    [MSG_UPDATING_DB]          = " ATUALIZANDO BANCO  ",
    [MSG_DB_SUCCESS]           = " BANCO ATUALIZADO   ",
    [MSG_SUCCESS]              = "Sucesso!            ",
    [MSG_WAIT]                 = "Aguarde...          ",
    [MSG_INIT_SCAN]            = "Iniciando scan...   ",
    [MSG_CONSOLE_ACTIVE]       = "CONSOLE ATIVO       ",
    [MSG_HOLD_BACK]            = "Segure 1s p/ voltar ",
    [MSG_SELECT_LETTER]        = " SELECIONE A LETRA  ",
    [MSG_LETTER_FORMAT]        = "-> LETRA %c (%d)",
    [MSG_NO_GAMES]             = "Sem jogos cadastrad.",
    [MSG_NAV_INSTRUCTION]      = "Gire: Nav Clique: OK",
    [MSG_NO_GAMES_AVAIL]       = "Sem jogos cadastrad.",
    [MSG_HOLD_BACK_GRID]       = "Segure 1s p/ voltar ",
    [MSG_LOADING_MENU]         = " CARREGANDO MENU... ",
    [MSG_QUICK_SETTINGS]       = "* CONFIG. RAPIDAS *",
    [MSG_MENU_BACK_TO_GAME]    = "Voltar ao Jogo",
    [MSG_MENU_RESTART]         = "Reiniciar",
    [MSG_MENU_CONTROLS]        = "Controles",
    [MSG_MENU_SCANLINES]       = "Scanlines",
    [MSG_MENU_DECOMB]          = "De-comb",
    [MSG_MENU_VOLUME]          = "Volume",
    [MSG_MENU_DIFF_P1]         = "Dificuldade P1",
    [MSG_MENU_DIFF_P2]         = "Dificuldade P2",
    [MSG_MENU_ASPECT]          = "Ajuste Tela",
    [MSG_MENU_SWAP_JOYS]       = "Swap Joysticks",
    [MSG_MENU_VIDEO_STD]       = "Padrao Video",
    [MSG_MENU_VBLANK]          = "VBlank",
    [MSG_MENU_VSYNC_STAB]      = "VSync Stab",
    [MSG_YES]                  = "Sim",
    [MSG_NO]                   = "Nao",
    [MSG_OFF]                  = "Desl.",
    [MSG_AUTO]                 = "AUTO",
    [MSG_MUTE]                 = "Mudo",
    [MSG_EASY]                 = "Facil",
    [MSG_HARD]                 = "Dificil",
    [MSG_NORMAL]               = "Normal",
    [MSG_SWAP_LABEL]           = "Trocar",
    [MSG_SMART]                = "Smart",
    [MSG_FIXED]                = "Fixed",
    [MSG_NONE]                 = "None"
};

static const char* const strings_en[MSG_COUNT] = {
    [MSG_DB_MENU_TITLE]        = "** SYSTEM MENU **   ",
    [MSG_DB_MENU_UPDATE]       = "Update Games",
    [MSG_DB_MENU_LANG]         = "Language: ",
    [MSG_DB_MENU_BACK]         = "Back",
    [MSG_READING_SD]           = "READING SD CARD...  ",
    [MSG_UPDATING_DB]          = " UPDATING DATABASE  ",
    [MSG_DB_SUCCESS]           = " DATABASE UPDATED   ",
    [MSG_SUCCESS]              = "Success!            ",
    [MSG_WAIT]                 = "Wait...             ",
    [MSG_INIT_SCAN]            = "Starting scan...    ",
    [MSG_CONSOLE_ACTIVE]       = "CONSOLE ACTIVE      ",
    [MSG_HOLD_BACK]            = "Hold 1s to go back  ",
    [MSG_SELECT_LETTER]        = " SELECT THE LETTER  ",
    [MSG_LETTER_FORMAT]        = "-> LETTER %c (%d)",
    [MSG_NO_GAMES]             = "No games registered.",
    [MSG_NAV_INSTRUCTION]      = "Turn: Nav Click: OK",
    [MSG_NO_GAMES_AVAIL]       = "No games registered.",
    [MSG_HOLD_BACK_GRID]       = "Hold 1s to go back  ",
    [MSG_LOADING_MENU]         = " LOADING MENU...    ",
    [MSG_QUICK_SETTINGS]       = "* QUICK SETTINGS * ",
    [MSG_MENU_BACK_TO_GAME]    = "Back to Game",
    [MSG_MENU_RESTART]         = "Restart",
    [MSG_MENU_CONTROLS]        = "Controls",
    [MSG_MENU_SCANLINES]       = "Scanlines",
    [MSG_MENU_DECOMB]          = "De-comb",
    [MSG_MENU_VOLUME]          = "Volume",
    [MSG_MENU_DIFF_P1]         = "Difficulty P1",
    [MSG_MENU_DIFF_P2]         = "Difficulty P2",
    [MSG_MENU_ASPECT]          = "Aspect Ratio",
    [MSG_MENU_SWAP_JOYS]       = "Swap Joysticks",
    [MSG_MENU_VIDEO_STD]       = "Video Standard",
    [MSG_MENU_VBLANK]          = "VBlank",
    [MSG_MENU_VSYNC_STAB]      = "VSync Stab",
    [MSG_YES]                  = "Yes",
    [MSG_NO]                   = "No",
    [MSG_OFF]                  = "Off",
    [MSG_AUTO]                 = "AUTO",
    [MSG_MUTE]                 = "Mute",
    [MSG_EASY]                 = "Easy",
    [MSG_HARD]                 = "Hard",
    [MSG_NORMAL]               = "Normal",
    [MSG_SWAP_LABEL]           = "Swap",
    [MSG_SMART]                = "Smart",
    [MSG_FIXED]                = "Fixed",
    [MSG_NONE]                 = "None"
};

void translation_set_language(Language lang) {
    if (lang < LANG_COUNT) {
        current_lang = lang;
    }
}

Language translation_get_language(void) {
    return current_lang;
}

const char* t(StringId id) {
    if (id < MSG_COUNT) {
        const char* str = (current_lang == LANG_EN) ? strings_en[id] : strings_pt[id];
        if (str != NULL) {
            return str;
        }
    }
    return "";
}
