#ifndef METRICAS_H
#define METRICAS_H

#include "hardware_specs.h"
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

typedef struct {
    uint32_t pid;
    uint64_t page_faults;
} MetricasProcesso;

typedef struct {
    uint64_t total_acessos;
    uint64_t total_page_faults;

    MetricasProcesso *processos;
    size_t quantidade_processos;
    size_t capacidade_processos;
    float temperatura_maxima;
    uint64_t ciclos_io;
} Metricas;

void inicializar_metricas(Metricas *metricas);

bool registrar_acesso(
    Metricas *metricas,
    uint32_t pid,
    bool houve_page_fault
);

void imprimir_metricas(const Metricas *metricas);

void liberar_metricas(Metricas *metricas);

bool registrar_operacao_swap(
    Metricas *metricas,
    const hardware_state_t *estado_antes,
    const hardware_state_t *estado_depois
);

#endif