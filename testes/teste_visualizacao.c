
#include <stdio.h>
#include <stdbool.h>
#include "visualizacao.h"

int main(void) {

    double fragmentacao;
    int aprovados = 0;

    printf("========== TESTES DE VISUALIZACAO ==========\n");

    // TESTE 1: Pagina par de 4 KB com 1 KB utilizado
    printf("\nTESTE 1 - Pagina par:\n");

    if (calcular_fragmentacao_pagina(0, 1024, &fragmentacao) &&
        fragmentacao == 75.0) {

        exibir_pagina_ascii(1, 0, 1024);
        printf("TESTE 1 APROVADO!\n");
        aprovados++;

    } else {
        printf("TESTE 1 FALHOU!\n");
    }

    // TESTE 2: Pagina impar de 8 KB com 1 KB utilizado
    printf("\nTESTE 2 - Pagina impar:\n");

    if (calcular_fragmentacao_pagina(1, 1024, &fragmentacao) &&
        fragmentacao == 87.5) {

        exibir_pagina_ascii(1, 1, 1024);
        printf("TESTE 2 APROVADO!\n");
        aprovados++;

    } else {
        printf("TESTE 2 FALHOU!\n");
    }

    // TESTE 3: Pagina totalmente utilizada
    printf("\nTESTE 3 - Pagina sem fragmentacao:\n");

    if (calcular_fragmentacao_pagina(2, 4096, &fragmentacao) &&
        fragmentacao == 0.0) {

        exibir_pagina_ascii(2, 2, 4096);
        printf("TESTE 3 APROVADO!\n");
        aprovados++;

    } else {
        printf("TESTE 3 FALHOU!\n");
    }

    // TESTE 4: Utilizacao parcial de 1 KB
    printf("\nTESTE 4 - Bloco parcialmente utilizado:\n");

    if (calcular_fragmentacao_pagina(0, 1536, &fragmentacao) &&
        fragmentacao == 62.5) {

        exibir_pagina_ascii(2, 0, 1536);
        printf("TESTE 4 APROVADO!\n");
        aprovados++;

    } else {
        printf("TESTE 4 FALHOU!\n");
    }

    // TESTE 5: Quantidade de bytes acima da capacidade
    printf("\nTESTE 5 - Quantidade invalida de bytes:\n");

    if (!calcular_fragmentacao_pagina(1, 8193, &fragmentacao)) {
        printf("TESTE 5 APROVADO: valor invalido rejeitado!\n");
        aprovados++;
    } else {
        printf("TESTE 5 FALHOU!\n");
    }

    // TESTE 6: Ponteiro nulo
    printf("\nTESTE 6 - Ponteiro nulo:\n");

    if (!calcular_fragmentacao_pagina(0, 1024, NULL)) {
        printf("TESTE 6 APROVADO: ponteiro nulo rejeitado!\n");
        aprovados++;
    } else {
        printf("TESTE 6 FALHOU!\n");
    }

    printf("\n========== RESULTADO FINAL ==========\n");
    printf("Testes aprovados: %d/6\n", aprovados);

    if (aprovados == 6) {
        printf("TODOS OS TESTES FORAM APROVADOS!\n");
        return 0;
    }

    return 1;
}
