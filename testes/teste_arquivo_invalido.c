
#include <stdio.h>
#include <stdbool.h>
#include <stddef.h>
#include "trace.h"

// Conta os acessos validos recebidos
bool contar_acesso(
    const AcessoMemoria *acesso,
    void *contexto
) {
    if (acesso == NULL || contexto == NULL) {
        return false;
    }

    size_t *contador = contexto;
    (*contador)++;

    return true;
}

int main(void) {

    size_t acessos_recebidos = 0;
    size_t total_processado = 0;

    bool sucesso = processar_arquivo_trace(
        "testes/trace_invalido.txt",
        contar_acesso,
        &acessos_recebidos,
        &total_processado
    );

    if (!sucesso &&
        total_processado == 1 &&
        acessos_recebidos == 1) {

        printf("TESTE APROVADO: linha invalida detectada!\n");
        printf("Leitura interrompida corretamente.\n");
        return 0;
    }

    printf("TESTE FALHOU!\n");
    return 1;
}
