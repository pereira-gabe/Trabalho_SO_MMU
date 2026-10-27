#include <stdio.h>
#include <inttypes.h>
#include "auditoria.h"

int main(void) {

    page_table_entry_t tabela[3] = {0};

    tabela[0].present = true;
    tabela[0].frame_number = 2;

    tabela[1].present = true;
    tabela[1].frame_number = 4;

    uint64_t hash1 = calcular_checksum(tabela, 3);

    printf("Checksum inicial: %" PRIu64 "\n", hash1);

    // Simulando alteracao na tabela
    tabela[1].frame_number = 7;

    uint64_t hash2 = calcular_checksum(tabela, 3);

    printf("Checksum atualizado: %" PRIu64 "\n", hash2);

    if (hash1 != hash2) {
        printf("TESTE APROVADO: alteracao detectada!\n");
    } else {
        printf("TESTE FALHOU: alteracao nao detectada.\n");
        return 1;
    }

    return 0;
}