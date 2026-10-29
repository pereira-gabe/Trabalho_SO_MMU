#include "auditoria.h"
#include <stdio.h>
#include <inttypes.h>
#include <stddef.h>

uint64_t calcular_checksum(
    const page_table_entry_t *tabela,
    uint32_t quantidade_paginas
) {
    if (tabela == NULL) {
        return 0;
    }

    uint64_t hash = 14695981039346656037ULL;

    for (uint32_t i = 0; i < quantidade_paginas; i++) {

        hash ^= i;
        hash *= 1099511628211ULL;

        hash ^= tabela[i].present ? 1ULL : 0ULL;
        hash *= 1099511628211ULL;

        hash ^= tabela[i].frame_number;
        hash *= 1099511628211ULL;
    }

    return hash;
}

// Checksum adicional do estado completo da tabela.
// Mantem o checksum estrutural original para compatibilidade.
uint64_t calcular_checksum_estado(
    const page_table_entry_t *tabela,
    uint32_t quantidade_paginas
) {
    if (tabela == NULL) {
        return 0;
    }

    uint64_t hash = calcular_checksum(tabela, quantidade_paginas);

    for (uint32_t i = 0; i < quantidade_paginas; i++) {
        hash ^= tabela[i].referenced ? 1ULL : 0ULL;
        hash *= 1099511628211ULL;

        hash ^= tabela[i].modified ? 1ULL : 0ULL;
        hash *= 1099511628211ULL;

        hash ^= tabela[i].last_access_t;
        hash *= 1099511628211ULL;
    }

    return hash;
}

void registrar_auditoria(
    const page_table_entry_t *tabela,
    uint32_t quantidade_paginas,
    uint32_t pid,
    const char *evento
) {
    if (tabela == NULL) {
        printf("[AUDITORIA] Erro: tabela invalida.\n");
        return;
    }

    uint64_t hash = calcular_checksum(tabela, quantidade_paginas);

    printf("\n[AUDITORIA] PID: %" PRIu32 "\n", pid);
    printf("[AUDITORIA] Evento: %s\n",
           evento != NULL ? evento : "Nao informado");
    printf("[AUDITORIA] Checksum estrutural: %" PRIu64 "\n", hash);
    printf("[AUDITORIA] Checksum estado: %" PRIu64 "\n",
           calcular_checksum_estado(tabela, quantidade_paginas));
}


void auditar_fading(
    const page_table_entry_t *antes,
    const page_table_entry_t *depois,
    uint32_t quantidade_paginas,
    uint32_t pid,
    uint64_t acessos_globais
) {
    if (antes == NULL || depois == NULL ||
        quantidade_paginas == 0) {
        return;
    }

    // O Fading ocorre a cada 5 acessos globais
    if (acessos_globais == 0 ||
        acessos_globais % FADING_THRESHOLD != 0) {
        return;
    }

    printf("\n========== FADING DO BIT R ==========\n");

    printf("[FADING] Acesso global: %" PRIu64 "\n",
           acessos_globais);

    printf("[FADING] PID: %" PRIu32 "\n", pid);

    uint32_t alteracoes = 0;

    // Compara os bits R antes e depois da chamada a MMU
    for (uint32_t i = 0; i < quantidade_paginas; i++) {

        if (antes[i].present &&
            depois[i].present &&
            antes[i].referenced &&
            !depois[i].referenced) {

            printf("[FADING] Bit R zerado na VPN %" PRIu32 "\n", i);

            alteracoes++;
        }
    }

    if (alteracoes == 0) {
        printf("[FADING] Nenhuma alteracao do bit R observada.\n");
    }

    // Checksum estrutural da tabela atual
    uint64_t checksum =
        calcular_checksum(depois, quantidade_paginas);

    printf("[AUDITORIA] Checksum estrutural: %" PRIu64 "\n",
           checksum);
    printf("[AUDITORIA] Checksum estado: %" PRIu64 "\n",
           calcular_checksum_estado(depois, quantidade_paginas));

    printf("====================================\n");
}
