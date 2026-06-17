# Especificação do Módulo de Controle do Sistema (sysctrl.v)

Este documento atua como referência técnica para a comunicação entre o RP2040 (Companion) e o módulo **`sysctrl.v`** da FPGA, responsável pela recepção de comandos de configuração geral do sistema (Target `0x00`), controle de LEDs, leitura de botões locais, e tratamento de interrupções e redirecionamento de porta serial.

---

## 1. Interface de Sinais (Ports) do Módulo

O módulo `sysctrl` possui as seguintes conexões principais:

*   **`clk`**: Clock principal do sistema FPGA.
*   **`reset`**: Sinal de reset global.
*   **Interface de dados (do módulo mcu_spi)**:
    *   `data_in_strobe`: Pulso que indica a chegada de um byte.
    *   `data_in_start`: Sinaliza o início da transação (Byte 0 do payload / ID do Comando).
    *   `data_in`: Byte recebido do MCU.
    *   `data_out`: Byte de resposta enviado de volta ao MCU.
*   **Interface de Interrupção**:
    *   `int_out_n`: Linha de interrupção física ativa em LOW (`int_in != 0` ou `sys_int == 1`).
    *   `int_in`: Vetor de interrupções externas.
    *   `int_ack`: Vetor de confirmação de interrupções enviado pelo MCU.
*   **Pinos Físicos & Periféricos**:
    *   `buttons` [1:0]: Estado dos botões locais da Tang Nano (S0 e S1).
    *   `leds` [1:0]: LEDs onboard controláveis pelo MCU.
    *   `color` [23:0]: Valor RGB de 24 bits para barramento do LED WS2812 (com inversão de bits).

---

## 2. Inicialização e Valores Padrão (Reset)

Ao receber o sinal de reset, o módulo define os seguintes estados iniciais padrão:

| Registrador | Valor Padrão (Binário) | Descrição |
| :--- | :---: | :--- |
| `leds` | `2'b00` | Ambos os LEDs desligados |
| `color` | `24'h000000` | Led RGB desligado (preto) |
| `main_reset` | `2'd3` | Sistema inicia em Reset (3 segundos) |
| `buttons_irq_enable`| `1'b1` | Interrupção de botões habilitada |
| `system_scanlines` | `2'b00` | Scanlines desativadas |
| `system_volume` | `2'b10` | Volume em 66% |
| `system_screen` | `2'b00` | Aspecto 4:3 |
| `system_port_1` | `4'b0000` | Seleção de periférico do Joystick P1 |
| `system_port_2` | `4'b0000` | Seleção de periférico do Joystick P2 |
| `system_video_std` | `2'b00` | Padrão de Vídeo: **AUTO** |
| `system_paddle` | `1'b0` | Paddle normal |
| `system_diff_p1` | `1'b0` | Dificuldade P1: B (Fácil) |
| `system_diff_p2` | `1'b0` | Dificuldade P2: B (Fácil) |
| `system_decomb` | `1'b0` | De-comb desativado |
| `system_vblank` | `1'b0` | VBlank desativado |
| `system_vm` | `1'b0` | Modo de cor: Colorido (0 = Color, 1 = B&W) |
| `system_sc` | `2'b11` | Superchip automático |
| `system_joyswap` | `1'b0` | Sem troca de joysticks |

---

## 3. Tabela de Comandos SPI (SYS Target - 0x00)

Sempre que a FPGA recebe um payload direcionado ao Target `0x00`, o byte seguinte ao ID do Target determina o ID do Comando (`command`):

### CMD 0: Solicitação de Status (SYS_STATUS)
*   **Retorno do Status**:
    *   Byte 0: Retorna `0x5C`
    *   Byte 1: Retorna `0x42`
    *   Byte 2: Retorna `0x00` (ID do Core Atari 2600)

### CMD 1: Controle de LEDs
*   **Payload**: `data_in[1:0]` controla diretamente o estado dos LEDs onboard da Tang Nano.

### CMD 2: Controle de LED RGB WS2812 (SPI_SYS_RGB)
*   **Payload**: Envia 3 bytes sequenciais correspondentes ao valor RGB.
*   **Bit-reversal**: O módulo inverte a ordem dos bits de cada byte de dados recebido (`data_in_rev`):
    *   Byte 0: `color[15:8] <= data_in_rev` (Green)
    *   Byte 1: `color[7:0]  <= data_in_rev` (Red)
    *   Byte 2: `color[23:16] <= data_in_rev` (Blue)

### CMD 3: Leitura de Botões Físicos
*   **Retorno**: `data_out <= { 6'b000000, buttons }`.
*   **Efeito colateral**: Reabilita a interrupção física por mudança nos botões (`buttons_irq_enable <= 1'b1`).

### CMD 4: Escrita de Configurações (SPI_SYS_SETVAL)
*   **Estrutura**:
    *   Byte de ID: Caractere ASCII que identifica a variável a ser alterada.
    *   Byte de Valor: Novo valor numérico (geralmente de 1 a 4 bits).

#### Variáveis Mapeadas no CMD 4:

| Caractere ID | Registrador Interno | Valores Mapeados / Descrição |
| :---: | :--- | :--- |
| **`"Q"`** | `system_port_1` | [3:0] Dispositivo de entrada na porta P1 |
| **`"J"`** | `system_port_2` | [3:0] Dispositivo de entrada na porta P2 |
| **`"&"`** | `system_joyswap` | [0] Inverter portas P1 e P2 (0 = Normal, 1 = Trocado) |
| **`"V"`** | `system_paddle` | [0] Inverter potenciômetro do Paddle |
| **`"X"`** | `system_diff_p1` | [0] Dificuldade P1 (0 = Fácil/B, 1 = Difícil/A) |
| **`"Y"`** | `system_diff_p2` | [0] Dificuldade P2 (0 = Fácil/B, 1 = Difícil/A) |
| **`"C"`** | `system_decomb` | [0] Filtro De-comb (0 = Off, 1 = On) |
| **`"M"`** | `system_vblank` | [0] Cortar vblank (0 = Off, 1 = On) |
| **`"O"`** | `system_vm` | [0] Modo de vídeo (0 = Colorido, 1 = Preto & Branco) |
| **`"U"`** | `system_sc` | [1:0] Configuração de Superchip (RAM extra) |
| **`"E"`** | `system_video_std` | [1:0] Padrão de Vídeo (**0 = AUTO**, **1 = PAL**, **2 = NTSC**) |
| **`"R"`** | `main_reset` | [1:0] Controle de Reset físico (1 = Reset ativo, 0 = Run) |
| **`"S"`** | `system_scanlines` | [1:0] Intensidade do Scanline (0 = Desl., 1 = 25%, 2 = 50%, 3 = 75%) |
| **`"A"`** | `system_volume` | [1:0] Volume (0 = Mudo, 1 = 33%, 2 = 66%, 3 = 100%) |
| **`"W"`** | `system_screen` | [1:0] Proporção da Tela (0 = Normal 4:3, 1 = Wide 16:9) |

---

### CMD 5: Tratamento de Interrupções
*   **Payload**: O primeiro byte recebido confirma/limpa as interrupções ativas (`int_ack <= data_in`).
*   **Retorno**: `data_out <= { int_in[7:1], sys_int }` (Bit 0 indica interrupção de sistema ativa - ex: boot).

### CMD 6: Leitura das Fontes de Interrupção
*   **Retorno**: `data_out <= { 5'b00000, !buttons_irq_enable, port_data_ready, coldboot }`.
*   **Efeito colateral**: A leitura do primeiro byte limpa o flag de coldboot (`coldboot <= 1'b0`).

---

### CMD 7: Redirecionamento da Porta Serial (MFP RS232)
Permite ao RP2040 ler/escrever dados do console serial simulado na FPGA.
*   **Byte 0**: Subcomando (`port_cmd`)
*   **Byte 1**: Índice da porta (`port_index`) - retorna `0` (serial)
*   **Subcomando `0` (Ler Status)**:
    *   Byte 2: Bytes disponíveis para leitura enviados pela FPGA (`port_out_available`)
    *   Byte 3: Espaço livre no buffer de recepção da FPGA (`port_in_available`)
    *   Bytes 4-7: Taxa de transmissão (baudrate) configurada no core e número de bits
*   **Subcomando `1` (Ler Dados)**: Retorna o dado do buffer de saída serial da FPGA (`port_out_data`).
*   **Subcomando `2` (Escrever Dados)**: Envia o byte `data_in` para a porta serial da FPGA.

---

### CMD 8: Leitura da Configuração XML do Menu (OSD)
*   **Retorno**: Retorna os bytes do arquivo de especificação do menu (`menu_rom_data`), incrementando sequencialmente o endereço de leitura da memória ROM interna (`menu_rom_addr`).
