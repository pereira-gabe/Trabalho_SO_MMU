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
    printf("[AUDITORIA] Checksum: %" PRIu64 "\n", hash);
}