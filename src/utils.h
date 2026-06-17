#ifndef UTILS_H
#define UTILS_H

#include <stdint.h>
#include <stddef.h>

// Contexto do acumulador de streaming MD5 (RFC 1321)
typedef struct {
    uint32_t state[4];   // Estado (A, B, C, D)
    uint32_t count[2];   // Número de bits analisados (modulo 2^64)
    uint8_t buffer[64];  // Buffer de dados de entrada
} MD5_CTX;

/**
 * @brief Inicializa o contexto MD5.
 */
void md5_init(MD5_CTX* context);

/**
 * @brief Adiciona dados de entrada para o cálculo acumulado do MD5.
 * @param context Ponteiro para o contexto MD5
 * @param input Dados a serem adicionados
 * @param input_len Comprimento dos dados
 */
void md5_update(MD5_CTX* context, const uint8_t* input, size_t input_len);

/**
 * @brief Finaliza o cálculo do MD5 e gera o digest de 16-bytes.
 * @param digest Retorno do hash bruto (16 bytes)
 * @param context Ponteiro para o contexto MD5
 */
void md5_final(uint8_t digest[16], MD5_CTX* context);

/**
 * @brief Utilitário rápido para converter um digest bruto de 16 bytes em string hexadecimal de 32 caracteres.
 * @param digest Digest bruto de 16 bytes
 * @param out_str Buffer de retorno contendo string (deve ter tamanho >= 33)
 */
void md5_to_str(const uint8_t digest[16], char out_str[33]);

/**
 * @brief Calcula o CRC32 de um buffer de dados de uma só vez.
 * @param data Ponteiro de dados
 * @param length Comprimento do buffer
 * @return CRC32 calculado
 */
uint32_t crc32_calculate(const uint8_t* data, size_t length);

#endif // UTILS_H
