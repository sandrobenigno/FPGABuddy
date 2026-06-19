#include "db.h"
#include "pico/stdlib.h"
#include "ff.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

// -------------------------------------------------------------
// Engine de SGBD Indexado O(1) de Alta Performance (Flat-File)
// -------------------------------------------------------------

// Estrutura do índice de busca rápida por categoria
typedef struct {
    uint32_t start; // Índice do primeiro registro do jogo na base oficial
    uint32_t count; // Quantidade de jogos nessa categoria
} LetterIndex;

// Estrutura do Header de 224 bytes
typedef struct {
    char magic[4];          // "ROM1"
    uint32_t version;       // Versão (1)
    LetterIndex index[27];  // Mapeamento para as 27 categorias (#, A-Z)
} DbHeader;

// Registro físico de tamanho fixo em arquivo (128 bytes)
typedef struct {
    char md5[32];        // MD5 hash em formato de string (sem null terminator)
    char nome[64];       // Nome do jogo padded com '\0'
    uint32_t tamanho;    // Tamanho do arquivo ROM em bytes
    char bank_model[16]; // Modelo de banking (16 caracteres)
    char letra;          // Letra para busca indexada rápida
    char padding[11];    // Alinhamento para atingir exatamente 128 bytes
} DbRecord;

static const char* OFFICIAL_DB_FILE_PATH = "/roms.bin";
static const char* USER_DB_FILE_PATH = "/user_games.bin";

static DbHeader db_header;
static bool db_initialized = false;

// Função de comparação insensível a maiúsculo/minúsculo para o qsort na RAM
static int game_name_compare(const void* a, const void* b) {
    const GameInfo* ga = (const GameInfo*)a;
    const GameInfo* gb = (const GameInfo*)b;
    
    const char* s1 = ga->nome;
    const char* s2 = gb->nome;
    while (*s1 && *s2) {
        char c1 = *s1;
        char c2 = *s2;
        if (c1 >= 'A' && c1 <= 'Z') c1 += 32;
        if (c2 >= 'A' && c2 <= 'Z') c2 += 32;
        if (c1 != c2) {
            return c1 - c2;
        }
        s1++;
        s2++;
    }
    char c1 = *s1;
    char c2 = *s2;
    if (c1 >= 'A' && c1 <= 'Z') c1 += 32;
    if (c2 >= 'A' && c2 <= 'Z') c2 += 32;
    return c1 - c2;
}

bool db_init(void) {
    db_initialized = false;
    FIL file;
    FRESULT fr = f_open(&file, OFFICIAL_DB_FILE_PATH, FA_READ);
    
    // Se o arquivo oficial não existir, cria uma base inicial estruturada para testes
    if (fr != FR_OK) {
        printf("[DB] Base oficial '%s' nao encontrada. Inicializando base indexada de teste...\n", OFFICIAL_DB_FILE_PATH);
        fr = f_open(&file, OFFICIAL_DB_FILE_PATH, FA_CREATE_ALWAYS | FA_WRITE | FA_READ);
        if (fr == FR_OK) {
            DbHeader test_header;
            memset(&test_header, 0, sizeof(DbHeader));
            memcpy(test_header.magic, "ROM1", 4);
            test_header.version = 1;
            
            // Registros de teste ordenados:
            // Letra 'A' (Índice 1): Adventure (A), Asteroids (A) -> start=0, count=2
            // Letra 'B' (Índice 2): Boxing (B) -> start=2, count=1
            // Letra 'C' (Índice 3): Combat (C) -> start=3, count=1
            test_header.index[1].start = 0;
            test_header.index[1].count = 2;
            test_header.index[2].start = 2;
            test_header.index[2].count = 1;
            test_header.index[3].start = 3;
            test_header.index[3].count = 1;
            
            DbRecord test_records[4] = {
                {
                    "e99a18c428cb38d5f260853678922e03",
                    "Adventure (A)",
                    4096,
                    "Standard 4KB",
                    'A',
                    {0}
                },
                {
                    "37f2ff841bc4d7a8d8e578c93de821b0",
                    "Asteroids (A)",
                    8192,
                    "F8 (8KB)",
                    'A',
                    {0}
                },
                {
                    "89a19c5b293883a681c2f90abef382b9",
                    "Boxing (B)",
                    2048,
                    "Standard 2KB",
                    'B',
                    {0}
                },
                {
                    "59a39d8b742cb3a90184bfaef382c49b",
                    "Combat (C)",
                    2048,
                    "Standard 2KB",
                    'C',
                    {0}
                }
            };
            
            UINT bw;
            f_write(&file, &test_header, sizeof(DbHeader), &bw);
            f_write(&file, test_records, sizeof(test_records), &bw);
            f_lseek(&file, 0); // Volta ao início para leitura
        } else {
            printf("[DB] Erro critico ao criar base oficial de teste (erro: %d)\n", fr);
            return false;
        }
    }
    
    // Lê o cabeçalho indexado de 224 bytes
    UINT br;
    fr = f_read(&file, &db_header, sizeof(DbHeader), &br);
    f_close(&file);
    
    if (fr != FR_OK || br != sizeof(DbHeader)) {
        printf("[DB] Falha ao ler cabecalho da base oficial (erro: %d)\n", fr);
        return false;
    }
    
    // Valida magic e version
    if (memcmp(db_header.magic, "ROM1", 4) != 0) {
        printf("[DB] Magic incorreto na base oficial: '%.4s'\n", db_header.magic);
        return false;
    }
    
    if (db_header.version != 1) {
        printf("[DB] Versao de banco incorreta: %d\n", db_header.version);
        return false;
    }
    
    printf("[DB] SGBD Indexado O(1) montado com sucesso (versao: %d).\n", db_header.version);
    db_initialized = true;
    return true;
}

bool db_get_available_letters(bool letras_disponiveis[27]) {
    memset(letras_disponiveis, 0, 27 * sizeof(bool));
    if (!db_initialized) return false;
    
    // 1. Marca as letras que têm jogos na base oficial de forma O(1) pelo Header
    for (int i = 0; i < 27; i++) {
        if (db_header.index[i].count > 0) {
            letras_disponiveis[i] = true;
        }
    }
    
    // 2. Marca as letras que têm jogos na base dinâmica do usuário
    FIL file;
    FRESULT fr = f_open(&file, USER_DB_FILE_PATH, FA_READ);
    if (fr == FR_OK) {
        DbRecord rec;
        UINT br;
        while (f_read(&file, &rec, sizeof(DbRecord), &br) == FR_OK && br == sizeof(DbRecord)) {
            char l = rec.letra;
            if (l == '#') {
                letras_disponiveis[0] = true;
            } else {
                if (l >= 'a' && l <= 'z') l -= 32;
                if (l >= 'A' && l <= 'Z') {
                    letras_disponiveis[l - 'A' + 1] = true;
                }
            }
        }
        f_close(&file);
    }
    
    return true;
}

bool db_fetch_games_by_letter(char letra, GameInfo** list, int* count) {
    if (!list || !count || !db_initialized) return false;
    
    *list = NULL;
    *count = 0;
    
    // 1. Mapeia o caractere da categoria para o índice correspondente no Header (0 a 26)
    int idx = 0;
    if (letra >= 'a' && letra <= 'z') letra -= 32;
    if (letra >= 'A' && letra <= 'Z') {
        idx = letra - 'A' + 1;
    } else {
        idx = 0; // '#'
    }
    
    uint32_t official_start = db_header.index[idx].start;
    uint32_t official_count = db_header.index[idx].count;
    
    // 2. Procura jogos correspondentes na base do usuário (/user_games.bin)
    uint32_t user_matching_count = 0;
    DbRecord user_temp_records[64]; // Buffer temporário seguro na pilha
    
    FIL file;
    FRESULT fr = f_open(&file, USER_DB_FILE_PATH, FA_READ);
    if (fr == FR_OK) {
        DbRecord rec;
        UINT br;
        while (f_read(&file, &rec, sizeof(DbRecord), &br) == FR_OK && br == sizeof(DbRecord)) {
            char l = rec.letra;
            if (l >= 'a' && l <= 'z') l -= 32;
            
            int rec_idx = 0;
            if (l >= 'A' && l <= 'Z') {
                rec_idx = l - 'A' + 1;
            } else {
                rec_idx = 0;
            }
            
            if (rec_idx == idx) {
                if (user_matching_count < 64) {
                    user_temp_records[user_matching_count] = rec;
                }
                user_matching_count++;
            }
        }
        f_close(&file);
    }
    
    uint32_t total_count = official_count + user_matching_count;
    if (total_count == 0) return true; // Sem jogos nesta categoria
    
    // 3. Aloca memória exata na heap para receber a lista fundida (zero-inicializada)
    *list = (GameInfo*)calloc(total_count, sizeof(GameInfo));
    if (!*list) {
        return false; // Heap Exhaustion
    }
    
    // 4. Carrega os registros oficiais diretamente via offset O(1) de busca rápida
    if (official_count > 0) {
        FIL official_file;
        fr = f_open(&official_file, OFFICIAL_DB_FILE_PATH, FA_READ);
        if (fr == FR_OK) {
            uint32_t offset = sizeof(DbHeader) + (official_start * sizeof(DbRecord));
            f_lseek(&official_file, offset);
            
            for (uint32_t i = 0; i < official_count; i++) {
                DbRecord official_rec;
                UINT br;
                FRESULT read_fr = f_read(&official_file, &official_rec, sizeof(DbRecord), &br);
                if (read_fr == FR_OK && br == sizeof(DbRecord)) {
                    memcpy((*list)[i].md5, official_rec.md5, 32);
                    (*list)[i].md5[32] = '\0';
                    
                    memcpy((*list)[i].nome, official_rec.nome, 63);
                    (*list)[i].nome[63] = '\0';
                    
                    (*list)[i].tamanho = official_rec.tamanho;
                    
                    memcpy((*list)[i].bank_model, official_rec.bank_model, 16);
                    (*list)[i].bank_model[16] = '\0';
                } else {
                    printf("[DB] ERRO: Falha ao ler registro oficial %d (FRESULT: %d, lidos: %d)\n", i, read_fr, br);
                    strcpy((*list)[i].nome, "Erro de Leitura SD");
                    memset((*list)[i].md5, 0, sizeof((*list)[i].md5));
                    (*list)[i].tamanho = 0;
                    strcpy((*list)[i].bank_model, "UNKNOWN");
                }
            }
            f_close(&official_file);
        }
    }
    
    // 5. Preenche os registros provenientes da base do usuário
    uint32_t user_loaded_limit = user_matching_count > 64 ? 64 : user_matching_count;
    for (uint32_t i = 0; i < user_loaded_limit; i++) {
        uint32_t list_idx = official_count + i;
        
        memcpy((*list)[list_idx].md5, user_temp_records[i].md5, 32);
        (*list)[list_idx].md5[32] = '\0';
        
        memcpy((*list)[list_idx].nome, user_temp_records[i].nome, 63);
        (*list)[list_idx].nome[63] = '\0';
        
        (*list)[list_idx].tamanho = user_temp_records[i].tamanho;
        
        memcpy((*list)[list_idx].bank_model, user_temp_records[i].bank_model, 16);
        (*list)[list_idx].bank_model[16] = '\0';
    }
    
    // Se o estouro de usuários exceder o limite seguro de 64, atualiza o total count real carregado
    if (user_matching_count > 64) {
        total_count = official_count + 64;
    }
    
    // 6. Roda um qsort na RAM ultra veloz para ordenar alfabeticamente os registros combinados
    qsort(*list, total_count, sizeof(GameInfo), game_name_compare);
    
    *count = total_count;
    return true;
}

void db_free_games_cache(GameInfo* list) {
    if (list) {
        free(list);
    }
}

bool db_check_game_exists(const char* md5) {
    if (!db_initialized) return false;
    
    // 1. Procura na base oficial de forma linear
    FIL file;
    FRESULT fr = f_open(&file, OFFICIAL_DB_FILE_PATH, FA_READ);
    if (fr == FR_OK) {
        f_lseek(&file, sizeof(DbHeader)); // Pula o cabeçalho indexado
        DbRecord rec;
        UINT br;
        while (f_read(&file, &rec, sizeof(DbRecord), &br) == FR_OK && br == sizeof(DbRecord)) {
            if (memcmp(rec.md5, md5, 32) == 0) {
                f_close(&file);
                return true;
            }
        }
        f_close(&file);
    }
    
    // 2. Procura na base dinâmica do usuário
    fr = f_open(&file, USER_DB_FILE_PATH, FA_READ);
    if (fr == FR_OK) {
        DbRecord rec;
        UINT br;
        while (f_read(&file, &rec, sizeof(DbRecord), &br) == FR_OK && br == sizeof(DbRecord)) {
            if (memcmp(rec.md5, md5, 32) == 0) {
                f_close(&file);
                return true;
            }
        }
        f_close(&file);
    }
    
    return false;
}

bool db_register_user_game(const char* md5, uint32_t size, const char* bank_model) {
    if (!db_initialized) return false;
    
    // 1. Evita gravação de jogos duplicados na base
    if (db_check_game_exists(md5)) {
        printf("[DB] Jogo com MD5 '%s' ja cadastrado no banco.\n", md5);
        return true;
    }
    
    // 2. Abre a base de usuário para escrita (cria se não existir)
    FIL file;
    FRESULT fr = f_open(&file, USER_DB_FILE_PATH, FA_OPEN_ALWAYS | FA_WRITE | FA_READ);
    if (fr != FR_OK) {
        printf("[DB] Falha ao abrir base de usuario '%s' (erro: %d)\n", USER_DB_FILE_PATH, fr);
        return false;
    }
    
    // 3. Calcula o número de sequência O(1) pelo tamanho do arquivo
    uint32_t file_size = f_size(&file);
    uint32_t user_index = (file_size / sizeof(DbRecord)) + 1;
    
    f_lseek(&file, file_size); // Apende no final do arquivo
    
    // 4. Monta o registro de 128 bytes
    DbRecord rec;
    memset(&rec, 0, sizeof(DbRecord));
    
    memcpy(rec.md5, md5, 32);
    snprintf(rec.nome, sizeof(rec.nome), "Jogo Usuario %03d", user_index);
    rec.tamanho = size;
    
    if (bank_model) {
        strncpy(rec.bank_model, bank_model, 15);
        rec.bank_model[15] = '\0';
    } else {
        strcpy(rec.bank_model, "Standard");
    }
    
    rec.letra = 'J'; // Agrupa dinamicamente sob o 'J' inicialmente
    
    // 5. Escreve no cartão SD
    UINT bw;
    fr = f_write(&file, &rec, sizeof(DbRecord), &bw);
    f_close(&file);
    
    if (fr == FR_OK && bw == sizeof(DbRecord)) {
        printf("[DB] Novo jogo de usuario registrado com SUCESSO: '%s' (MD5: %s)\n", rec.nome, md5);
        return true;
    } else {
        printf("[DB] Falha ao gravar registro na base de usuario (erro: %d)\n", fr);
        return false;
    }
}

void db_close(void) {
    db_initialized = false;
}

