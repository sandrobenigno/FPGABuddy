#include "encoder.h"
#include "pico/stdlib.h"
#include "hardware/pio.h"
#include "encoder.pio.h" // Cabecalho autogerado pelo pioasm no CMake

static uint8_t last_state = 0;

void encoder_init(void) {
    // 1. Inicializa o botão de clique do encoder (ENC_SW) com pull-up interno
    gpio_init(ENC_SW_PIN);
    gpio_set_dir(ENC_SW_PIN, GPIO_IN);
    gpio_pull_up(ENC_SW_PIN);

    // Inicializa pino LED onboard para mostrar o clique
    gpio_init(ONBOARD_LED_PIN);
    gpio_set_dir(ONBOARD_LED_PIN, GPIO_OUT);

    // 2. Adiciona o programa PIO no bloco pio0
    uint offset = pio_add_program(ENC_PIO, &encoder_program);
    
    // 3. Inicializa os pinos de barramento da máquina de estados do PIO0 SM1
    encoder_program_init(ENC_PIO, ENC_SM, offset, ENC_A_PIN);

    // 4. Carrega o estado de quadratura inicial (Gray Code)
    uint8_t a = gpio_get(ENC_A_PIN);
    uint8_t b = gpio_get(ENC_B_PIN);
    last_state = (a << 1) | b;

    // 5. Habilita a máquina de estados do Encoder
    pio_sm_set_enabled(ENC_PIO, ENC_SM, true);
}

int32_t encoder_get_rotation(void) {
    int32_t steps = 0;
    static int32_t accumulator = 0;

    // Tabela de transição de quadratura
    // Índice: [estado_anterior (2 bits) | estado_atual (2 bits)]
    // Valores de retorno: +1 (horário), -1 (anti-horário), 0 (sem movimento / inválido)
    static const int8_t transition_table[16] = {
        0,  1, -1,  0, // old = 00: to 00(0), 01(1), 10(-1), 11(0)
       -1,  0,  0,  1, // old = 01: to 00(-1), 01(0), 10(0), 11(1)
        1,  0,  0, -1, // old = 10: to 00(1), 01(0), 10(0), 11(-1)
        0, -1,  1,  0  // old = 11: to 00(0), 01(-1), 10(1), 11(0)
    };

    // Lê todas as transições pendentes acumuladas no FIFO RX do PIO
    while (!pio_sm_is_rx_fifo_empty(ENC_PIO, ENC_SM)) {
        // Os bits significativos de quadratura estão nos bits 0 e 1 (LSB)
        uint32_t raw_val = pio_sm_get(ENC_PIO, ENC_SM);
        uint8_t current_state = raw_val & 0x03;

        // Decodifica a transição de Gray Code
        int8_t movement = transition_table[(last_state << 2) | current_state];
        steps += movement;
        last_state = current_state;
    }

    // Acumula os passos físicos. Encoders mecânicos EC11 geram 4 transições de estado (passos de quadratura) por clique físico.
    accumulator += steps;
    int32_t clicks = accumulator / 4;
    accumulator %= 4;

    return clicks;
}

typedef enum {
    BTN_STATE_UP,
    BTN_STATE_DEBOUNCE_DOWN,
    BTN_STATE_DOWN,
    BTN_STATE_LONG_PRESSED
} BtnState;

ButtonEvent encoder_check_button(void) {
    static BtnState state = BTN_STATE_UP;
    static uint32_t press_start_time = 0;
    static uint32_t last_state_change = 0;
    
    uint32_t now = to_ms_since_boot(get_absolute_time());
    
    // Ativo em LOW para o botão do encoder (GP15)
    bool is_down = (gpio_get(ENC_SW_PIN) == 0); 
    
    // Mostra o status do botão no LED onboard
    gpio_put(ONBOARD_LED_PIN, is_down);
    
    ButtonEvent event = BUTTON_NONE;
    
    switch (state) {
        case BTN_STATE_UP:
            if (is_down) {
                state = BTN_STATE_DEBOUNCE_DOWN;
                last_state_change = now;
            }
            break;
            
        case BTN_STATE_DEBOUNCE_DOWN:
            if (is_down) {
                if (now - last_state_change >= DEBOUNCE_ENC_BTN_MS) { // Debounce configurado
                    state = BTN_STATE_DOWN;
                    press_start_time = now;
                }
            } else {
                state = BTN_STATE_UP;
            }
            break;
            
        case BTN_STATE_DOWN:
            if (!is_down) {
                // Soltou antes do tempo de pressionamento longo (1000ms)
                if (now - press_start_time >= 20) { // Garante duração mínima de 20ms após debounce
                    event = BUTTON_CLICK;
                }
                state = BTN_STATE_UP;
            } else {
                // Continua pressionado -> Verifica se atingiu 1 segundo
                if (now - press_start_time >= 1000) {
                    event = BUTTON_LONG_PRESS;
                    state = BTN_STATE_LONG_PRESSED;
                }
            }
            break;
            
        case BTN_STATE_LONG_PRESSED:
            if (!is_down) {
                // Aguarda soltar para voltar ao estado padrão
                state = BTN_STATE_UP;
            }
            break;
    }
    
    return event;
}
