
#include "visualizacao.h"
#include "hardware_specs.h"

#include <stdio.h>
#include <inttypes.h>

bool calcular_fragmentacao_pagina(
    uint32_t vpn,
    uint32_t bytes_utilizados,
    double *percentual
) {
    uint32_t tamanho_pagina =
        (vpn % 2 == 0) ? PAGE_SIZE_EVEN : PAGE_SIZE_ODD;

    if (percentual == NULL ||
        bytes_utilizados > tamanho_pagina) {
        return false;
    }

    *percentual =
        100.0 * (tamanho_pagina - bytes_utilizados)
        / tamanho_pagina;

    return true;
}

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

    // Cada caractere representa 1 KB
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
