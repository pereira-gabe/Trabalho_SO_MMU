
#include <stdio.h>
#include <stdbool.h>
#include "visualizacao.h"
#include "hardware_specs.h"

int main(void) {

    // RAM de 256 KB dividida em 64 blocos de 4 KB
    BlocoRamVisual ram[64] = {0};

    printf("========== TESTE DO MAPA DA RAM ==========\n");

    // Pagina par (4 KB), utilizando apenas 1 KB
    ram[0].ocupado = true;
    ram[0].pid = 1;
    ram[0].vpn = 0;
    ram[0].bytes_utilizados = 1024;

    // Pagina impar (8 KB), ocupando dois blocos
    // Apenas 1 KB utilizado no primeiro bloco
    ram[1].ocupado = true;
    ram[1].pid = 1;
    ram[1].vpn = 1;
    ram[1].bytes_utilizados = 1024;

    ram[2].ocupado = true;
    ram[2].pid = 1;
    ram[2].vpn = 1;
    ram[2].bytes_utilizados = 0;

    // Outra pagina par, totalmente utilizada
    ram[3].ocupado = true;
    ram[3].pid = 2;
    ram[3].vpn = 2;
    ram[3].bytes_utilizados = 4096;

    // TESTE 1: Exibir mapa completo
    printf("\nTESTE 1 - MAPA COMPLETO:\n");

    if (exibir_mapa_ram_ascii(ram, 64)) {
        printf("TESTE 1 APROVADO!\n");
    } else {
        printf("TESTE 1 FALHOU!\n");
        return 1;
    }

    // TESTE 2: Quantidade incorreta de blocos
    printf("\nTESTE 2 - QUANTIDADE INVALIDA:\n");

    if (!exibir_mapa_ram_ascii(ram, 63)) {
        printf("TESTE 2 APROVADO!\n");
    } else {
        printf("TESTE 2 FALHOU!\n");
        return 1;
    }

    // TESTE 3: Bloco com mais de 4 KB utilizados
    printf("\nTESTE 3 - CAPACIDADE EXCEDIDA:\n");

    ram[0].bytes_utilizados = 5000;

    if (!exibir_mapa_ram_ascii(ram, 64)) {
        printf("TESTE 3 APROVADO!\n");
    } else {
        printf("TESTE 3 FALHOU!\n");
        return 1;
    }

    // Restaurar valor original
    ram[0].bytes_utilizados = 1024;

    // TESTE 4: Bloco livre com bytes utilizados
    printf("\nTESTE 4 - BLOCO LIVRE INVALIDO:\n");

    ram[4].ocupado = false;
    ram[4].bytes_utilizados = 1024;

    if (!exibir_mapa_ram_ascii(ram, 64)) {
        printf("TESTE 4 APROVADO!\n");
    } else {
        printf("TESTE 4 FALHOU!\n");
        return 1;
    }

    printf("\nTODOS OS TESTES FORAM APROVADOS!\n");

    return 0;
}
