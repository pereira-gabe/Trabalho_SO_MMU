#ifndef RAM_H
#define RAM_H

#include "mmu.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* Um bloco de 4 KB da RAM fisica. Pagina de 8 KB ocupa dois blocos contiguos
 * com o mesmo (pid, vpn). */
typedef struct {
    bool ocupado;
    uint32_t pid;
    uint32_t vpn;
    uint32_t bytes_utilizados;
} ram_bloco_t;

typedef struct {
    ram_bloco_t blocos[MMU_NUM_BLOCOS_RAM];
} ram_t;

/* Tabela de paginas (MMU_NUM_PAGINAS entradas) de um processo. */
typedef struct {
    uint32_t pid;
    const page_table_entry_t *tabela;
} ram_processo_t;

void ram_inicializar(ram_t *ram);

/* Primeiro bloco de uma sequencia livre de 1 ou 2 blocos contiguos. */
bool ram_buscar_livre(const ram_t *ram, uint32_t blocos, uint32_t *frame);

/* Ocupa os blocos da pagina a partir de frame, repartindo bytes_utilizados
 * (1 a tamanho da pagina) entre eles. Nao sobrescreve blocos ocupados. */
bool ram_alocar(ram_t *ram, uint32_t frame, uint32_t pid, uint32_t vpn,
                uint32_t bytes_utilizados);

/* Libera a pagina que ocupa frame, que pode ser qualquer um dos seus blocos.
 * pid e vpn sao opcionais e recebem a identidade da pagina liberada. */
bool ram_liberar(ram_t *ram, uint32_t frame, uint32_t *pid, uint32_t *vpn);

uint32_t ram_blocos_ocupados(const ram_t *ram);
uint32_t ram_maior_livre_contiguo(const ram_t *ram);

/* Verifica que a RAM e as tabelas descrevem o mesmo mapeamento: todo bloco
 * ocupado e reivindicado por uma PTE presente, e toda PTE presente aponta
 * para blocos ocupados pela propria pagina. Em caso de falha, motivo (se nao
 * NULL) recebe a descricao do primeiro problema encontrado. */
bool ram_validar(const ram_t *ram, const ram_processo_t *processos,
                 size_t quantidade, const char **motivo);

#endif
