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

#endif