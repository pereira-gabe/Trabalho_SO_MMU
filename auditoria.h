#ifndef AUDITORIA_H
#define AUDITORIA_H

#include <stdint.h>
#include "hardware_specs.h"

uint64_t calcular_checksum(
    const page_table_entry_t *tabela,
    uint32_t quantidade_paginas
);

void registrar_auditoria(
    const page_table_entry_t *tabela,
    uint32_t quantidade_paginas,
    uint32_t pid,
    const char *evento
);

void auditar_fading(
    const page_table_entry_t *antes,
    const page_table_entry_t *depois,
    uint32_t quantidade_paginas,
    uint32_t pid,
    uint64_t acessos_globais
);

#endif