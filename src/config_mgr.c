#include "config_mgr.h"
#include "translation.h"
#include "ff.h"
#include <stdio.h>
#include <string.h>

#define CONFIG_FILE_PATH "/fpgabuddy.cfg"

void config_load(void) {
    FIL file;
    FRESULT fr = f_open(&file, CONFIG_FILE_PATH, FA_READ);
    if (fr == FR_OK) {
        char buffer[64];
        UINT br;
        fr = f_read(&file, buffer, sizeof(buffer) - 1, &br);
        if (fr == FR_OK && br > 0) {
            buffer[br] = '\0';
            char *line = strtok(buffer, "\r\n");
            while (line != NULL) {
                if (strncmp(line, "lang=", 5) == 0) {
                    if (strcmp(line + 5, "en") == 0) {
                        translation_set_language(LANG_EN);
                    } else {
                        translation_set_language(LANG_PT_BR);
                    }
                }
                line = strtok(NULL, "\r\n");
            }
        }
        f_close(&file);
    } else {
        // Se o arquivo não existir, cria um novo com as configurações padrão
        config_save();
    }
}

void config_save(void) {
    FIL file;
    FRESULT fr = f_open(&file, CONFIG_FILE_PATH, FA_WRITE | FA_CREATE_ALWAYS);
    if (fr == FR_OK) {
        char line[64];
        Language lang = translation_get_language();
        snprintf(line, sizeof(line), "lang=%s\n", lang == LANG_EN ? "en" : "pt");
        
        UINT bw;
        f_write(&file, line, strlen(line), &bw);
        f_close(&file);
    }
}
