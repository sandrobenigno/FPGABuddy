#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "pico/stdlib.h"
#include "hardware/spi.h"
#include "lcd_20x4.h"
#include "ff.h"
#include "f_util.h"
#include "db.h"
#include "encoder.h"
#include "fpga_ctrl.h"
#include "ui_menu.h"

// Define o pino Chip Select da FPGA no barramento SPI0 nativo compartilhado
#define CS_FPGA_PIN    17
#define SD_CS_PIN      22

// Estados globais da máquina de estados do sistema
typedef enum {
    STATE_SELECIONANDO,
    STATE_JOGANDO,
    STATE_CONFIGURANDO
} SystemState;

// Sub-estados para a tela de seleção
typedef enum {
    SUBSTATE_LETTER_GRID,
    SUBSTATE_GAME_LIST
} SelectionSubState;

// Objeto de sistema de arquivos global do FatFs
static FATFS fs;

// Função nativa do SDK para verificar conexão USB CDC
extern bool stdio_usb_connected(void);

static char active_game_name[32] = "";

int main() {
    // 1. Inicializa IMEDIATAMENTE os pinos de Chip Select para desativar o barramento compartilhado e evitar conflitos
    fpga_ctrl_init_pins();

    // 2. Inicializa a telemetria serial (USB CDC e UART0)
    stdio_init_all();
    
    // Aguarda de forma inteligente até que o monitor serial no PC seja aberto (máximo 3 segundos)
    int usb_wait_timeout = 30; // 30 * 100ms = 3 segundos
    while (!stdio_usb_connected() && usb_wait_timeout > 0) {
        sleep_ms(100);
        usb_wait_timeout--;
    }
    
    // Delay de estabilização pós-conexão
    sleep_ms(500);
    
    printf("\n============================================\n");
    printf("[DEBUG] RP2040 Companion: Iniciando...\n");

    // 2.5. Configura GP12 como VCC temporário para o encoder (Saída HIGH)
    gpio_init(12);
    gpio_set_dir(12, GPIO_OUT);
    gpio_put(12, 1);

    // 2.7. DIAGNÓSTICO DE BARRAMENTO I2C FÍSICO (Verifica pull-ups externos reais)
    gpio_init(LCD_I2C_SDA_PIN);
    gpio_init(LCD_I2C_SCL_PIN);
    gpio_set_dir(LCD_I2C_SDA_PIN, GPIO_IN);
    gpio_set_dir(LCD_I2C_SCL_PIN, GPIO_IN);
    
    // Desliga pull-ups/pull-downs internos para ver se o Level Shifter/Pull-up externo está alimentando a linha
    gpio_disable_pulls(LCD_I2C_SDA_PIN);
    gpio_disable_pulls(LCD_I2C_SCL_PIN);
    sleep_ms(20); // Aguarda descarga de capacitância
    
    bool sda_physical = gpio_get(LCD_I2C_SDA_PIN);
    bool scl_physical = gpio_get(LCD_I2C_SCL_PIN);
    
    printf("[DIAGNOSTICO I2C] Sem pull-ups internos: SDA (GP%d) = %d | SCL (GP%d) = %d\n", 
           LCD_I2C_SDA_PIN, sda_physical, LCD_I2C_SCL_PIN, scl_physical);
           
    if (!sda_physical || !scl_physical) {
        printf("[ERRO CRITICO] Linha(s) sem pull-up externo ativo! O Level Shifter LV (3.3V) pode estar sem alimentacao ou desconectado.\n");
    } else {
        printf("[DIAGNOSTICO I2C] Pull-ups externos detectados com SUCESSO! Alimentacao do Level Shifter OK.\n");
    }

    // 3. EXECUTAR I2C SCANNER (GP20 e GP21)
    i2c_init(LCD_I2C_INST, LCD_I2C_BAUDRATE);
    gpio_set_function(LCD_I2C_SDA_PIN, GPIO_FUNC_I2C);
    gpio_set_function(LCD_I2C_SCL_PIN, GPIO_FUNC_I2C);
    // Para a operação normal do I2C do RP2040, mantemos os pullups internos habilitados em paralelo
    gpio_pull_up(LCD_I2C_SDA_PIN);
    gpio_pull_up(LCD_I2C_SCL_PIN);

    printf("[I2C SCANNER] Escaneando barramento I2C0...\n");
    int found_count = 0;
    for (int addr = 1; addr < 128; addr++) {
        uint8_t dummy = 0;
        int ret = i2c_write_timeout_us(LCD_I2C_INST, addr, &dummy, 1, false, 10000); // 10ms timeout
        if (ret >= 0) {
            printf("[I2C SCANNER] --> Dispositivo no endereco: 0x%02X\n", addr);
            found_count++;
        }
    }

    // 4. Inicializa o LCD
    bool lcd_ok = false;
    if (found_count > 0) {
        lcd_ok = lcd_init();
    }
    ui_menu_init(lcd_ok);

    if (!lcd_ok) {
        printf("[ERRO] Display LCD nao respondendo.\n");
    }

    // 5. Inicializa o Rotary Encoder (PIO0 SM1 e Pino 13)
    printf("[DEBUG] Inicializando Rotary Encoder...\n");
    encoder_init();
    printf("[DEBUG] Rotary Encoder configurado.\n");

    // 6. Inicializa o Cartão SD (SPI0 e FatFs)
    printf("[DEBUG] Inicializando Cartao SD...\n");
    sleep_ms(1000); // Aguarda estabilizacao da tensao e do SD Card
    claim_spi_bus(); // Garante que o barramento SPI0 esteja inicializado e ativo
    FRESULT fr = f_mount(&fs, "", 1);
    bool sd_ok = (fr == FR_OK);
    bool db_ok = false;

    if (sd_ok) {
        printf("[DEBUG] Cartao SD montado com sucesso!\n");
        db_ok = db_init();
        if (db_ok) {
            printf("[DEBUG] Banco de dados '/roms.bin' inicializado com sucesso!\n");
        } else {
            printf("[ERRO] Falha ao abrir '/roms.bin' no cartao SD.\n");
        }
    } else {
        printf("[ERRO] Falha ao inicializar o Cartao SD (f_mount falhou: %d - %s)\n", fr, FRESULT_str(fr));
    }

    // Inicializa LED Onboard
    const uint LED_PIN = PICO_DEFAULT_LED_PIN;
    gpio_init(LED_PIN);
    gpio_set_dir(LED_PIN, GPIO_OUT);

    // Inicializa o botao auxiliar GP3 (entrada com pull-down interno)
    gpio_init(3);
    gpio_set_dir(3, GPIO_IN);
    gpio_pull_down(3);

    // 7. Teste de Diagnóstico da FPGA (SPI_SYS_STATUS)
    printf("[DEBUG_FPGA] Testando comunicacao com a FPGA via SPI (GP%d)...\n", CS_FPGA_PIN);
    spi_set_baudrate(spi0, 5000000); // 5 MHz
    
    gpio_put(CS_FPGA_PIN, 0); // Seleciona a FPGA
    sleep_us(10);
    
    uint8_t tx_buf[4] = {0x00, 0x00, 0x00, 0x00}; // Target 0x00, Cmd 0x00, 2 dummy
    uint8_t rx_buf[4] = {0x00, 0x00, 0x00, 0x00};
    spi_write_read_blocking(spi0, tx_buf, rx_buf, 4);
    
    sleep_us(10);
    gpio_put(CS_FPGA_PIN, 1); // Deseleciona a FPGA
    
    printf("[DEBUG_FPGA] Status retornado: [0]:0x%02X | [1]:0x%02X | [2]:0x%02X | [3]:0x%02X\n",
           rx_buf[0], rx_buf[1], rx_buf[2], rx_buf[3]);
    
    if (rx_buf[2] == 0x5C || rx_buf[3] == 0x5C || rx_buf[2] == 0x42 || rx_buf[3] == 0x42 ||
        rx_buf[0] == 0x21 || rx_buf[1] == 0x21 || rx_buf[3] == 0x2E) {
        printf("[DEBUG_FPGA] SUCESSO! FPGA respondeu corretamente ao protocolo SPI.\n");
        // Força as configurações padrão no boot do sistema (em SPI Mode 1)
        fpga_send_all_configs();
        fpga_set_rgb(0, 127, 0); // Verde no início do estado de Seleção
        // Restaura o formato SPI para o modo esperado pelo cartão SD (SPI Mode 0)
        spi_set_format(spi0, 8, SPI_CPOL_0, SPI_CPHA_0, SPI_MSB_FIRST);
    } else {
        printf("[DEBUG_FPGA] AVISO! Resposta inesperada ou nula da FPGA. Verifique a fiação e o Level Shifter.\n");
    }

    // Estado lógico global
    SystemState current_state = STATE_SELECIONANDO;
    SelectionSubState sel_substate = SUBSTATE_LETTER_GRID;

    int category_idx = 0; // Começa na letra 'A' (índice 0)
    
    // Variáveis para o Menu de Configurações Rápidas (Quick Settings)
    bool edit_mode = false;
    int menu_focus = 0;
    int menu_scroll = 0;
    uint32_t last_blink_time = 0;
    bool blink_state = true;
    
    // Cache dinâmico da listagem de jogos da letra atual
    GameInfo* game_list = NULL;
    int game_count = 0;
    int selected_idx = 0;

    // Inicializa a tela com o Grid de Letras se o banco estiver operante
    uint32_t last_ui_blink_time = 0;
    bool ui_blink_visible = true;
    
    ui_draw_message(" SELECIONE A LETRA  ", NULL, NULL, NULL);
    ui_draw_letters_grid_full(category_idx);

    printf("\n============================================\n");
    printf("[SISTEMA] Sistema operando no estado: SELECIONANDO (Grade de Letras)\n");
    printf("[SISTEMA] Barramento SPI0 ativo por conta do Cartao SD.\n");
    printf("============================================\n\n");

    while (true) {
        uint32_t now = to_ms_since_boot(get_absolute_time());
        
        if (current_state == STATE_SELECIONANDO) {
            // Polling de rotação e botão
            int32_t rot = encoder_get_rotation();
            ButtonEvent btn_event = encoder_check_button();
            
            if (sel_substate == SUBSTATE_LETTER_GRID) {
                // ---------------------------------------------------------
                // MODO: GRADE DE LETRAS (A até Z, #)
                // ---------------------------------------------------------
                if (rot != 0) {
                    int prev_idx = category_idx;
                    category_idx += rot;
                    if (category_idx < 0) category_idx = 26;
                    if (category_idx > 26) category_idx = 0;
                    
                    // Restaura a letra anterior para visível e limpa colchetes
                    ui_draw_grid_char(prev_idx, true, false);
                    
                    // Força visibilidade imediata no movimento e zera o tempo de blink
                    ui_blink_visible = true;
                    last_ui_blink_time = now;
                    
                    printf("[GRADE] Navegando: Letra '%c'\n", CATEGORIES[category_idx]);
                    ui_draw_grid_char(category_idx, true, true);
                }
                
                // Rotina de piscar a letra selecionada a cada 250ms de forma não-bloqueante
                if (now - last_ui_blink_time >= 250) {
                    last_ui_blink_time = now;
                    ui_blink_visible = !ui_blink_visible;
                    ui_draw_grid_char(category_idx, ui_blink_visible, true);
                }
                
                if (btn_event == BUTTON_CLICK) {
                    char current_letter = CATEGORIES[category_idx];
                    printf("[GRADE] Letra '%c' confirmada. Carregando lista...\n", current_letter);
                    
                    if (db_ok) {
                        db_free_games_cache(game_list);
                        game_list = NULL;
                        game_count = 0;
                        selected_idx = 0;
                        
                        db_fetch_games_by_letter(current_letter, &game_list, &game_count);
                        
                        // Transiciona para a lista de jogos
                        sel_substate = SUBSTATE_GAME_LIST;
                        
                        char title[21];
                        snprintf(title, sizeof(title), "-> LETRA %c (%d)", current_letter, game_count);
                        
                        if (game_count > 0) {
                            char line[21];
                            snprintf(line, sizeof(line), "%s", game_list[selected_idx].nome);
                            ui_draw_message(title, "====================", line, "Gire: Nav | Clique: OK");
                        } else {
                            ui_draw_message(title, "====================", "Sem jogos cadastrad.", "Segure 1s p/ voltar ");
                        }
                    }
                }
            } 
            else if (sel_substate == SUBSTATE_GAME_LIST) {
                // ---------------------------------------------------------
                // MODO: LISTAGEM DE JOGOS
                // ---------------------------------------------------------
                if (rot != 0 && game_count > 0) {
                    selected_idx += rot;
                    if (selected_idx < 0) selected_idx = 0;
                    if (selected_idx >= game_count) selected_idx = game_count - 1;
                    
                    printf("[MENU] Selecionando jogo: [%d] %s\n", selected_idx + 1, game_list[selected_idx].nome);
                    
                    if (ui_is_lcd_ok()) {
                        char line[21];
                        snprintf(line, sizeof(line), "%s", game_list[selected_idx].nome);
                        lcd_write_line(2, line);
                    }
                }
                
                if (btn_event == BUTTON_CLICK) {
                    // Confirma a injeção do jogo selecionado
                    if (db_ok && game_count > 0) {
                        GameInfo game = game_list[selected_idx];
                        printf("[SISTEMA] Selecao confirmada: %s (MD5: %s | Banking: %s)\n", game.nome, game.md5, game.bank_model);
                        snprintf(active_game_name, sizeof(active_game_name), "%s", game.nome);
                        
                        // Desaloca o cache local de lista para liberar RAM antes da injeção
                        db_free_games_cache(game_list);
                        game_list = NULL;
                        game_count = 0;

                        if (fpga_inject_rom(&game)) {
                            // Transiciona para o estado JOGANDO
                            current_state = STATE_JOGANDO;
                            fpga_set_rgb(0, 0, 127); // Azul ao concluir o carregamento da ROM
                            printf("\n============================================\n");
                            printf("[SISTEMA] Estado alterado para: JOGANDO\n");
                            printf("[SISTEMA] SPI ativa, console rodando da RAM.\n");
                            printf("============================================\n\n");
                            
                            ui_draw_message("CONSOLE ATIVO       ", "====================", active_game_name, "Segure 1s p/ voltar ");
                        } else {
                            // Re-carrega a lista de jogos para poder navegar novamente
                            db_fetch_games_by_letter(CATEGORIES[category_idx], &game_list, &game_count);
                            selected_idx = 0;
                            sel_substate = SUBSTATE_GAME_LIST;
                            current_state = STATE_SELECIONANDO;
                            
                            // Redesenha a lista
                            char title[21];
                            snprintf(title, sizeof(title), "-> LETRA %c (%d)", CATEGORIES[category_idx], game_count);
                            if (game_count > 0) {
                                char line[21];
                                snprintf(line, sizeof(line), "%s", game_list[selected_idx].nome);
                                ui_draw_message(title, "====================", line, "Gire: Nav | Clique: OK");
                            } else {
                                ui_draw_message(title, "====================", "Sem jogos cadastrad.", "Segure 1s p/ voltar ");
                            }
                        }
                    }
                }
                
                if (btn_event == BUTTON_LONG_PRESS) {
                    // Clique longo (1s) na lista de jogos -> Volta para a grade de letras!
                    printf("[SISTEMA] Clique longo detectado na lista! Voltando para o Grid de Letras...\n");
                    sel_substate = SUBSTATE_LETTER_GRID;
                    
                    ui_draw_message(" SELECIONE A LETRA  ", NULL, NULL, NULL);
                    ui_draw_letters_grid_full(category_idx);
                }
            }
            
            sleep_ms(5);
        } else if (current_state == STATE_JOGANDO || current_state == STATE_CONFIGURANDO) {
            ButtonEvent btn_event = encoder_check_button();
            
            if (btn_event == BUTTON_LONG_PRESS && current_state == STATE_JOGANDO) {
                printf("[SISTEMA] Clique longo de 1s! Solicitando retorno ao menu...\n");
                
                // Força a saída do modo de edição se estiver nele
                edit_mode = false;
                
                ui_draw_message(" CARREGANDO MENU... ", "====================", "Aguarde...          ", "                    ");
                
                // fpga_reset_and_eject(); // Desativado para manter o jogo rodando na FPGA ao voltar ao menu
                
                fpga_set_rgb(0, 127, 0); // Verde antes de retornar para o estado de Seleção
                
                // Remonta o Cartão SD e reabre o Banco de Dados
                printf("[SISTEMA] Reativando barramento SPI0 para o Cartao SD...\n");
                
                restore_sd_spi();
                
                fr = f_mount(&fs, "", 1);
                sd_ok = (fr == FR_OK);
                if (sd_ok) {
                    db_ok = db_init();
                } else {
                    printf("[ERRO] Falha ao remontar o Cartao SD (f_mount falhou: %d - %s)\n", fr, FRESULT_str(fr));
                }
                
                // Se remontado com sucesso, recarrega a listagem de jogos da letra atual
                if (db_ok) {
                    char current_letter = CATEGORIES[category_idx];
                    db_fetch_games_by_letter(current_letter, &game_list, &game_count);
                    
                    // Transiciona de volta para a lista de jogos!
                    sel_substate = SUBSTATE_GAME_LIST;
                    current_state = STATE_SELECIONANDO;
                    
                    char title[21];
                    snprintf(title, sizeof(title), "-> LETRA %c (%d)", current_letter, game_count);
                    if (game_count > 0) {
                        char line[21];
                        snprintf(line, sizeof(line), "%s", game_list[selected_idx].nome);
                        ui_draw_message(title, "====================", line, "Gire: Nav | Clique: OK");
                    } else {
                        ui_draw_message(title, "====================", "Sem jogos cadastrad.", "Segure 1s p/ voltar ");
                    }
                }
                
                printf("\n============================================\n");
                printf("[SISTEMA] Estado alterado para: SELECIONANDO (Lista de Jogos)\n");
                printf("[SISTEMA] Barramento SPI0 ativo.\n");
                printf("============================================\n\n");
            } else {
                // Cliques normais ou Rotações
                if (current_state == STATE_JOGANDO) {
                    // Polling do botão auxiliar GP3 para F1 Select (com 30ms de debounce)
                    static bool last_gp3_state = false;
                    static uint32_t last_gp3_change = 0;
                    bool gp3_state = gpio_get(3);
                    if (gp3_state != last_gp3_state && (now - last_gp3_change > 30)) {
                        last_gp3_state = gp3_state;
                        last_gp3_change = now;
                        fpga_send_key(0x3a, gp3_state); // F1 Select (0x3a)
                        printf("[SISTEMA] GP3 alterou para %d. Enviando tecla F1 Select...\n", gp3_state);
                    }

                    if (btn_event == BUTTON_CLICK) {
                        // Transiciona para o menu de configurações rápidas!
                        current_state = STATE_CONFIGURANDO;
                        fpga_set_rgb(127, 0, 0); // Vermelho ao entrar no estado de Configuração
                        edit_mode = false;
                        menu_focus = 1; // Inicia selecionado em "Reiniciar"
                        menu_scroll = 0;
                        
                        ui_draw_message("* CONFIG. RAPIDAS *", NULL, NULL, NULL);
                        for (int i = 0; i < 3; i++) {
                            ui_draw_quick_settings_line(i + 1, menu_scroll + i, (menu_focus == menu_scroll + i), false, true);
                        }
                        printf("[SISTEMA] Entrando no Menu de Configuracoes Rapidas (Foco em Reiniciar)\n");
                    }
                } else if (current_state == STATE_CONFIGURANDO) {
                    int32_t rot = encoder_get_rotation();
                    
                    if (!edit_mode) {
                        // MODO NAVEGAÇÃO
                        if (rot != 0) {
                            int prev_focus = menu_focus;
                            menu_focus += rot;
                            if (menu_focus < 0) menu_focus = 0;
                            if (menu_focus >= MENU_MAX_ITEMS) menu_focus = MENU_MAX_ITEMS - 1;
                            
                            if (menu_focus != prev_focus) {
                                // Atualiza o viewport de scroll
                                if (menu_focus < menu_scroll) {
                                    menu_scroll = menu_focus;
                                } else if (menu_focus >= menu_scroll + 3) {
                                    menu_scroll = menu_focus - 2;
                                }
                                
                                // Redesenha a lista do menu
                                for (int i = 0; i < 3; i++) {
                                    int item_idx = menu_scroll + i;
                                    ui_draw_quick_settings_line(i + 1, item_idx, (menu_focus == item_idx), false, true);
                                }
                            }
                        }
                        
                        if (btn_event == BUTTON_CLICK) {
                            if (menu_focus == 0) {
                                // Voltar ao Jogo
                                current_state = STATE_JOGANDO;
                                fpga_set_rgb(0, 0, 127); // Azul ao voltar para o jogo
                                ui_draw_message("CONSOLE ATIVO       ", "====================", active_game_name, "Segure 1s p/ voltar ");
                                printf("[SISTEMA] Voltando para a gameplay\n");
                            } else if (menu_focus == 1) {
                                // Reiniciar
                                printf("[SISTEMA] Reiniciando o console...\n");
                                fpga_reset_core();
                                current_state = STATE_JOGANDO;
                                fpga_set_rgb(0, 0, 127); // Azul ao voltar para o jogo
                                ui_draw_message("CONSOLE ATIVO       ", "====================", active_game_name, "Segure 1s p/ voltar ");
                            } else {
                                // Entra no modo de edição da opção
                                edit_mode = true;
                                blink_state = true;
                                last_blink_time = now;
                                int row = 1 + (menu_focus - menu_scroll);
                                ui_draw_quick_settings_line(row, menu_focus, true, true, blink_state);
                                printf("[SISTEMA] Editando item: %s\n", ui_get_menu_option(menu_focus)->label);
                            }
                        }
                    } else {
                        // MODO EDIÇÃO
                        if (rot != 0) {
                            MenuOption* opt = ui_get_menu_option(menu_focus);
                            int val = (int)opt->value + rot;
                            if (val < 0) val = 0;
                            if (val > opt->max_val) val = opt->max_val;
                            
                            if (val != opt->value) {
                                opt->value = (uint8_t)val;
                                blink_state = true; // Força visualização ao alterar
                                last_blink_time = now;
                                int row = 1 + (menu_focus - menu_scroll);
                                ui_draw_quick_settings_line(row, menu_focus, true, true, blink_state);
                            }
                        }
                        
                        if (btn_event == BUTTON_CLICK) {
                            // Confirma o valor e envia via SPI para a FPGA
                            MenuOption* opt = ui_get_menu_option(menu_focus);
                            uint8_t target_val = opt->value;
                            char target_id = opt->id;
                            
                            printf("[SPI] Enviando config '%c' = %d para a FPGA...\n", target_id, target_val);
                            fpga_send_config(target_id, target_val);
                            
                            // Sai do modo de edição
                            edit_mode = false;
                            int row = 1 + (menu_focus - menu_scroll);
                            ui_draw_quick_settings_line(row, menu_focus, true, false, true);
                            printf("[SISTEMA] Valor confirmado para %s: %s\n", opt->label, opt->value_labels[target_val]);
                        }
                    }
                }
            }
            
            sleep_ms(5);
        }
    }

    return 0;
}
