#include "auditoria.h"
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