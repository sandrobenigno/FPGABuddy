#ifndef ENCODER_H
#define ENCODER_H

#include <stdbool.h>
#include <stdint.h>
#include "hardware/pio.h"
#include "config.h"

#define ENC_PIO        pio0
#define ENC_SM         1

/**
 * @brief Inicializa o driver do rotary encoder (PIO0 SM1) e do botao físico (GPIO 13).
 */
void encoder_init(void);

/**
 * @brief Lê de forma nao-bloqueante a rotacao acumulada desde a ultima chamada.
 * @return Deslocamento relativo da rotacao (+1 para horario, -1 anti-horario, 0 sem movimento)
 */
int32_t encoder_get_rotation(void);

typedef enum {
    BUTTON_NONE,
    BUTTON_CLICK,
    BUTTON_LONG_PRESS
} ButtonEvent;

/**
 * @brief Verifica eventos de botão do encoder de forma robusta e com debouncing.
 * Distingue clique curto de clique longo (1 segundo).
 * @return O evento detectado (BUTTON_NONE, BUTTON_CLICK, ou BUTTON_LONG_PRESS)
 */
ButtonEvent encoder_check_button(void);

#endif // ENCODER_H
