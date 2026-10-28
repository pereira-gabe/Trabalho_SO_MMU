
#include "metricas.h"
#include <stdio.h>
#include <stdlib.h>
#include <inttypes.h>

// Inicializa as estatisticas
void inicializar_metricas(Metricas *metricas) {
    if (metricas == NULL) {
        return;
    }

    *metricas = (Metricas){0};
    metricas->temperatura_maxima = BASE_SWAP_TEMP;
}

// Registra um acesso a memoria
bool registrar_acesso(
    Metricas *metricas,
    uint32_t pid,
    bool houve_page_fault
) {
    if (metricas == NULL) {
        return false;
    }

    size_t i;

    // Procura o processo pelo PID
    for (i = 0; i < metricas->quantidade_processos; i++) {
        if (metricas->processos[i].pid == pid) {
            break;
        }
    }

    // Se o processo ainda nao existe, cadastra
    if (i == metricas->quantidade_processos) {

        if (metricas->quantidade_processos ==
            metricas->capacidade_processos) {

            size_t nova_capacidade =
                metricas->capacidade_processos == 0
                ? 4
                : metricas->capacidade_processos * 2;

            MetricasProcesso *novo = realloc(
                metricas->processos,
                nova_capacidade * sizeof(MetricasProcesso)
            );

            if (novo == NULL) {
                return false;
            }

            metricas->processos = novo;
            metricas->capacidade_processos = nova_capacidade;
        }

        metricas->processos[i].pid = pid;
        metricas->processos[i].page_faults = 0;
        metricas->quantidade_processos++;
    }

    metricas->total_acessos++;

    if (houve_page_fault) {
        metricas->total_page_faults++;
        metricas->processos[i].page_faults++;
    }

    return true;
}

// Registra temperatura e ciclos de uma operacao de Swap
bool registrar_operacao_swap(
    Metricas *metricas,
    const hardware_state_t *estado_antes,
    const hardware_state_t *estado_depois
) {
    if (metricas == NULL ||
        estado_antes == NULL ||
        estado_depois == NULL) {
        return false;
    }

    if (estado_depois->total_cpu_cycles <
        estado_antes->total_cpu_cycles) {
        return false;
    }

    uint64_t custo =
        estado_depois->total_cpu_cycles -
        estado_antes->total_cpu_cycles;

    metricas->ciclos_io += custo;

    // Atualiza a temperatura maxima
    if (estado_depois->swap_temperature >
        metricas->temperatura_maxima) {

        metricas->temperatura_maxima =
            estado_depois->swap_temperature;
    }

    return true;
}

// Registra uma amostra de fragmentacao interna
bool registrar_fragmentacao(
    Metricas *metricas,
    uint32_t bytes_alocados,
    uint32_t bytes_utilizados
) {
    if (metricas == NULL ||
        bytes_alocados == 0 ||
        bytes_alocados > PHYSICAL_RAM_SIZE ||
        bytes_utilizados > bytes_alocados) {
        return false;
    }

    double fragmentacao =
        100.0 * (bytes_alocados - bytes_utilizados)
        / bytes_alocados;

    metricas->soma_fragmentacao += fragmentacao;
    metricas->amostras_fragmentacao++;

    return true;
}

// Imprime as estatisticas finais
void imprimir_metricas(const Metricas *metricas) {
    if (metricas == NULL) {
        return;
    }

    printf("\n========== METRICAS ==========\n");

    printf("Total de acessos: %" PRIu64 "\n",
           metricas->total_acessos);

    printf("Page Faults globais: %" PRIu64 "\n",
           metricas->total_page_faults);

    printf("\nPage Faults por processo:\n");

    for (size_t i = 0;
         i < metricas->quantidade_processos;
         i++) {

        printf("PID %" PRIu32 ": %" PRIu64 "\n",
               metricas->processos[i].pid,
               metricas->processos[i].page_faults);
    }

    printf("\nTemperatura maxima do Swap: %.2f C\n",
           metricas->temperatura_maxima);

    printf("Ciclos gastos em E/S: %" PRIu64 "\n",
           metricas->ciclos_io);

    // Calcula e imprime a fragmentacao media
    if (metricas->amostras_fragmentacao > 0) {

        double media =
            metricas->soma_fragmentacao /
            metricas->amostras_fragmentacao;

        printf("Fragmentacao interna media: %.2f%%\n",
               media);

    } else {
        printf("Fragmentacao interna media: sem amostras\n");
    }

    printf("==============================\n");
}

// Libera a memoria alocada
void liberar_metricas(Metricas *metricas) {
    if (metricas == NULL) {
        return;
    }

    free(metricas->processos);
    *metricas = (Metricas){0};
}
