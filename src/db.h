#ifndef DB_H
#define DB_H

#include <stdbool.h>
#include <stdint.h>

// Estrutura representando informações básicas de um jogo
typedef struct {
    char md5[33];        // MD5 (32 caracteres + null terminator)
    char nome[64];       // Nome do jogo (exibido na UI)
    uint32_t tamanho;    // Tamanho do arquivo ROM em bytes
    char bank_model[17]; // Modelo de banking (16 caracteres + null terminator)
} GameInfo;

/**
 * @brief Inicializa e abre o banco de dados/índice de jogos no cartão SD.
 * @return true se aberto com sucesso, false caso contrário.
 */
bool db_init(void);

/**
 * @brief Verifica quais letras do alfabeto possuem jogos cadastrados.
 * @param letras_disponiveis Vetor de 27 booleanos a ser preenchido (true se houver jogos para a letra).
 * @return true se bem-sucedido.
 */
bool db_get_available_letters(bool letras_disponiveis[27]);

/**
 * @brief Carrega na RAM os jogos de uma letra específica (Cache de Letra Única).
 * @param letra Caractere da letra selecionada ('A' a 'Z').
 * @param list Ponteiro para retornar o vetor de GameInfo alocado dinamicamente.
 * @param count Retorno do número de jogos encontrados.
 * @return true se bem-sucedido.
 */
bool db_fetch_games_by_letter(char letra, GameInfo** list, int* count);

/**
 * @brief Libera a memória alocada pelo cache dinâmico de jogos.
 * @param list Vetor de GameInfo a ser liberado.
 */
void db_free_games_cache(GameInfo* list);

/**
 * @brief Verifica se um jogo já está cadastrado no banco por meio de seu MD5.
 * @param md5 String contendo o hash MD5 (32 caracteres).
 * @return true se o jogo já existe, false se não existir.
 */
bool db_check_game_exists(const char* md5);

/**
 * @brief Registra um novo jogo de dump de usuário no arquivo `/user_games.bin`.
 * Evita duplicatas se o MD5 já estiver cadastrado na base oficial ou de usuário.
 * @param md5 Hash MD5 do jogo (32 caracteres).
 * @param size Tamanho do jogo em bytes.
 * @param bank_model Nome do modelo de banking (máx 16 caracteres).
 * @return true se registrado com sucesso ou se já existia.
 */
bool db_register_user_game(const char* md5, uint32_t size, const char* bank_model);

/**
 * @brief Fecha a conexão com o banco de dados.
 */
void db_close(void);

#endif // DB_H
