
#include "visualizacao.h"
#include "hardware_specs.h"

#include <stdio.h>
#include <inttypes.h>

// Verifica se a VPN inteira cabe no espaco virtual assimetrico.
static bool vpn_valida(uint32_t vpn) {
    uint64_t inicio = (uint64_t)(vpn / 2) *
                      (PAGE_SIZE_EVEN + PAGE_SIZE_ODD);

    if (vpn % 2 != 0) {
        inicio += PAGE_SIZE_EVEN;
    }

    uint64_t tamanho = (vpn % 2 == 0)
                       ? PAGE_SIZE_EVEN : PAGE_SIZE_ODD;

    return inicio + tamanho <= VIRTUAL_MEMORY_SIZE;
}

// Calcula a fragmentacao interna de uma pagina
bool calcular_fragmentacao_pagina(
    uint32_t vpn,
    uint32_t bytes_utilizados,
    double *percentual
) {
    uint32_t tamanho_pagina =
        (vpn % 2 == 0) ? PAGE_SIZE_EVEN : PAGE_SIZE_ODD;

    if (percentual == NULL ||
        !vpn_valida(vpn) ||
        bytes_utilizados > tamanho_pagina) {
        return false;
    }

    *percentual =
        100.0 * (tamanho_pagina - bytes_utilizados)
        / tamanho_pagina;

    return true;
}

// Exibe uma pagina individual em ASCII
bool exibir_pagina_ascii(
    uint32_t pid,
    uint32_t vpn,
    uint32_t bytes_utilizados
) {
    uint32_t tamanho_pagina =
        (vpn % 2 == 0) ? PAGE_SIZE_EVEN : PAGE_SIZE_ODD;

    double fragmentacao;

    if (!calcular_fragmentacao_pagina(
            vpn, bytes_utilizados, &fragmentacao)) {
        return false;
    }

    printf("PID %" PRIu32 " | VPN %" PRIu32 " | ",
           pid, vpn);

    printf("%" PRIu32 " KB | [", tamanho_pagina / 1024);

    for (uint32_t i = 0; i < tamanho_pagina; i += 1024) {

        if (bytes_utilizados >= i + 1024) {
            printf("U");
        } else if (bytes_utilizados > i) {
            printf("P");
        } else {
            printf("F");
        }
    }

    printf("] Fragmentacao: %.2f%%\n", fragmentacao);

    return true;
}

// Exibe o mapa completo da memoria RAM
bool exibir_mapa_ram_ascii(
    const BlocoRamVisual *ram,
    size_t quantidade_blocos
) {
    const size_t total_esperado =
        PHYSICAL_RAM_SIZE / PAGE_SIZE_EVEN;

    if (ram == NULL || quantidade_blocos != total_esperado) {
        return false;
    }

    // Valida todos os blocos antes da exibicao
    for (size_t i = 0; i < quantidade_blocos; i++) {

        if (ram[i].bytes_utilizados > PAGE_SIZE_EVEN) {
            return false;
        }

        if (ram[i].ocupado && !vpn_valida(ram[i].vpn)) {
            return false;
        }

        if (!ram[i].ocupado &&
            ram[i].bytes_utilizados != 0) {
            return false;
        }
    }

    size_t blocos_ocupados = 0;
    size_t bytes_utilizados_total = 0;

    printf("\n========== MAPA DA RAM ==========\n");

    // Exibe oito blocos por linha
    for (size_t i = 0; i < quantidade_blocos; i += 8) {

        printf("Frames %02zu-%02zu: ", i, i + 7);

        for (size_t j = i; j < i + 8; j++) {

            printf("[");

            if (!ram[j].ocupado) {

                printf("....");

            } else {

                blocos_ocupados++;
                bytes_utilizados_total +=
                    ram[j].bytes_utilizados;

                // Cada caractere representa 1 KB
                for (uint32_t k = 0;
                     k < PAGE_SIZE_EVEN;
                     k += 1024) {

                    if (ram[j].bytes_utilizados >= k + 1024) {
                        printf("U");

                    } else if (ram[j].bytes_utilizados > k) {
                        printf("P");

                    } else {
                        printf("F");
                    }
                }
            }

            printf("]");
        }

        printf("\n");
    }

    size_t bytes_alocados =
        blocos_ocupados * PAGE_SIZE_EVEN;

    double fragmentacao = 0.0;

    if (bytes_alocados > 0) {
        fragmentacao =
            100.0 * (bytes_alocados - bytes_utilizados_total)
            / bytes_alocados;
    }

    printf("\nBlocos ocupados: %zu/%zu\n",
           blocos_ocupados, quantidade_blocos);

    printf("Memoria alocada: %zu KB\n",
           bytes_alocados / 1024);

    printf("Memoria utilizada: %zu bytes\n",
           bytes_utilizados_total);

    printf("Fragmentacao interna: %.2f%%\n",
           fragmentacao);

    printf("\nLegenda:\n");
    printf("U = Utilizado\n");
    printf("P = Parcialmente utilizado\n");
    printf("F = Fragmentacao interna\n");
    printf(". = Memoria livre\n");

    printf("================================\n");

    return true;
}
