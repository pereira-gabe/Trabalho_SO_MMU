#ifndef MMU_H
#define MMU_H

#include "hardware_specs.h"

#include <stdbool.h>
#include <stdint.h>

/* Paginas pares tem 4 KB e impares 8 KB, entao o padrao se repete a cada
 * 12 KB. 1 MB comporta 85 pares completos e uma ultima pagina de 4 KB que
 * fecha o espaco exatamente: 171 paginas, VPN 0 a 170. */
#define MMU_BYTES_PAR       (PAGE_SIZE_EVEN + PAGE_SIZE_ODD)
#define MMU_NUM_PAGINAS     ((uint32_t)((VIRTUAL_MEMORY_SIZE / MMU_BYTES_PAR) * 2 + \
                            ((VIRTUAL_MEMORY_SIZE % MMU_BYTES_PAR) >= PAGE_SIZE_EVEN ? 1 : 0)))
#define MMU_ULTIMA_VPN      (MMU_NUM_PAGINAS - 1U)

/* A RAM e dividida em blocos de 4 KB; uma pagina de 8 KB ocupa dois blocos
 * contiguos. frame_number, na PTE, e o indice do primeiro bloco da pagina. */
#define MMU_BLOCO_BYTES     PAGE_SIZE_EVEN
#define MMU_NUM_BLOCOS_RAM  ((uint32_t)(PHYSICAL_RAM_SIZE / MMU_BLOCO_BYTES))

typedef struct {
    uint32_t endereco_virtual;
    uint32_t vpn;
    uint32_t offset;
    uint32_t tamanho_pagina;
    bool presente;
    uint32_t endereco_fisico; /* so vale se presente */
} mmu_traducao_t;

uint32_t mmu_tamanho_pagina(uint32_t vpn);
uint32_t mmu_blocos_da_pagina(uint32_t vpn);
bool mmu_vpn_valida(uint32_t vpn);
bool mmu_base_virtual(uint32_t vpn, uint32_t *base);

/* Falha, sem tocar nas saidas, se o endereco passar de 1 MB. */
bool mmu_traduzir_virtual(uint32_t endereco_virtual, uint32_t *vpn, uint32_t *offset);

/* Falha se a pagina estiver ausente, se o offset passar do tamanho da pagina
 * ou se a pagina nao couber inteira nos 256 KB a partir do frame_number. */
bool mmu_traduzir_fisico(const page_table_entry_t *tabela, uint32_t vpn,
                         uint32_t offset, uint32_t *endereco_fisico);

/* tabela pode ser NULL (a pagina e tratada como ausente). Falha se o endereco
 * for invalido ou se a PTE presente apontar para fora da RAM. */
bool mmu_traduzir(const page_table_entry_t *tabela, uint32_t endereco_virtual,
                  mmu_traducao_t *resultado);

void mmu_imprimir_traducao(uint32_t pid, const mmu_traducao_t *traducao);

#endif
