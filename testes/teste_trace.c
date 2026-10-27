
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include "trace.h"

int main(void) {

    // Linhas de teste e resultados esperados
    struct {
        const char *linha;
        bool valido;
    } testes[] = {
        {"0 1 R 0", true},
        {"200 1 W 12288", true},
        {"201 2 R 1048575", true},
        {"15 3 W 4096", true},
        {"10 1 X 4096", false},
        {"10 1 R 1048576", false},
        {"10 1 R -1", false},
        {"10 1 R 4096 extra", false},
        {"10 1 4096", false},
        {"4294967296 1 R 0", false}
    };

    int quantidade = sizeof(testes) / sizeof(testes[0]);
    int aprovados = 0;

    for (int i = 0; i < quantidade; i++) {

        AcessoMemoria acesso = {0};

        bool resultado = interpretar_linha_trace(
            testes[i].linha, &acesso
        );

        printf("\nTESTE %d: %s\n", i + 1, testes[i].linha);

        if (resultado == testes[i].valido) {
            printf("APROVADO!\n");
            aprovados++;

            if (resultado) {
                printf("Timestamp: %u\n", acesso.timestamp);
                printf("PID: %u\n", acesso.pid);
                printf("Operacao: %c\n", acesso.operacao);
                printf("Endereco: %u\n", acesso.endereco);
            }

        } else {
            printf("FALHOU!\n");
        }
    }

    printf("\n========== RESULTADO ==========\n");
    printf("Testes aprovados: %d/%d\n", aprovados, quantidade);

    if (aprovados == quantidade) {
        printf("TODOS OS TESTES FORAM APROVADOS!\n");
        return 0;
    }

    return 1;
}
