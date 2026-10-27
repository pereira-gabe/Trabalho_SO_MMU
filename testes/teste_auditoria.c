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

    // TESTE 1: Alterar o numero do frame
    tabela[1].frame_number = 7;

    uint64_t hash2 = calcular_checksum(tabela, 3);

    if (hash1 != hash2) {
        printf("TESTE 1 APROVADO: alteracao do frame detectada!\n");
    } else {
        printf("TESTE 1 FALHOU!\n");
        return 1;
    }

    // TESTE 2: Alterar o bit present
    tabela[2].present = true;
    tabela[2].frame_number = 9;

    uint64_t hash3 = calcular_checksum(tabela, 3);

    if (hash2 != hash3) {
        printf("TESTE 2 APROVADO: nova pagina detectada!\n");
    } else {
        printf("TESTE 2 FALHOU!\n");
        return 1;
    }

    // TESTE 3: Remover uma pagina da RAM
    tabela[2].present = false;

    uint64_t hash4 = calcular_checksum(tabela, 3);

    if (hash3 != hash4) {
        printf("TESTE 3 APROVADO: remocao detectada!\n");
    } else {
        printf("TESTE 3 FALHOU!\n");
        return 1;
    }

    // TESTE 4: Verificar estabilidade do checksum
    uint64_t hash5 = calcular_checksum(tabela, 3);

    if (hash4 == hash5) {
        printf("TESTE 4 APROVADO: checksum consistente!\n");
    } else {
        printf("TESTE 4 FALHOU!\n");
        return 1;
    }

// TESTE 5: Verificar os logs de auditoria

printf("\n--- TESTE 5: LOGS DE AUDITORIA ---\n");

registrar_auditoria(tabela, 3, 1, "Estado inicial");

uint64_t hash_antes = calcular_checksum(tabela, 3);

// Simulando uma alteracao na tabela
tabela[0].frame_number = 5;

registrar_auditoria(tabela, 3, 1, "Frame da pagina 0 alterado");

uint64_t hash_depois = calcular_checksum(tabela, 3);

if (hash_antes != hash_depois) {
    printf("TESTE 5 APROVADO: checksums diferentes!\n");
} else {
    printf("TESTE 5 FALHOU!\n");
    return 1;
}

    printf("\nTODOS OS TESTES FORAM APROVADOS!\n");

    return 0;
}