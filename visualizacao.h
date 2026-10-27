
#ifndef VISUALIZACAO_H
#define VISUALIZACAO_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

// Representacao de um bloco fisico de 4 KB
typedef struct {
    bool ocupado;
    uint32_t pid;
    uint32_t vpn;
    uint32_t bytes_utilizados;
} BlocoRamVisual;

// Calcula a fragmentacao de uma pagina
bool calcular_fragmentacao_pagina(
    uint32_t vpn,
    uint32_t bytes_utilizados,
    double *percentual
);

// Exibe uma pagina individual
bool exibir_pagina_ascii(
    uint32_t pid,
    uint32_t vpn,
    uint32_t bytes_utilizados
);

// Exibe o mapa completo dos 256 KB
bool exibir_mapa_ram_ascii(
    const BlocoRamVisual *ram,
    size_t quantidade_blocos
);

#endif
