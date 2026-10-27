
#ifndef VISUALIZACAO_H
#define VISUALIZACAO_H

#include <stdint.h>
#include <stdbool.h>

bool calcular_fragmentacao_pagina(
    uint32_t vpn,
    uint32_t bytes_utilizados,
    double *percentual
);

bool exibir_pagina_ascii(
    uint32_t pid,
    uint32_t vpn,
    uint32_t bytes_utilizados
);

#endif
