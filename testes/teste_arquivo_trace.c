
#include <stdio.h>
#include <stdbool.h>
#include <stdint.h>
#include <inttypes.h>
#include "trace.h"

// Acessos esperados
static const AcessoMemoria esperados[] = {
    {0, 1, 'R', 0},
    {1, 1, 'W', 12288},
    {2, 2, 'R', 4096},
    {3, 1, 'W', 8192},
    {4, 2, 'R', 1048575}
};

typedef struct {
    size_t acessos_verificados;
} ContextoTeste;

// Verifica cada acesso lido
bool verificar_acesso(
    const AcessoMemoria *acesso,
    void *contexto
) {
    ContextoTeste *teste = contexto;

    size_t i = teste->acessos_verificados;

    if (i >= 5) {
        return false;
    }

    printf("\nAcesso %zu:\n", i + 1);
    printf("Timestamp: %" PRIu32 "\n", acesso->timestamp);
    printf("PID: %" PRIu32 "\n", acesso->pid);
    printf("Operacao: %c\n", acesso->operacao);
    printf("Endereco: %" PRIu32 "\n", acesso->endereco);

    if (acesso->timestamp != esperados[i].timestamp ||
        acesso->pid != esperados[i].pid ||
        acesso->operacao != esperados[i].operacao ||
        acesso->endereco != esperados[i].endereco) {

        printf("ERRO: dados diferentes do esperado!\n");
        return false;
    }

    teste->acessos_verificados++;

    printf("Acesso validado!\n");
    return true;
}

int main(void) {

    ContextoTeste contexto = {0};
    size_t total_processado = 0;

    bool sucesso = processar_arquivo_trace(
        "testes/trace_exemplo.txt",
        verificar_acesso,
        &contexto,
        &total_processado
    );

    if (!sucesso) {
        printf("\nTESTE FALHOU: erro na leitura!\n");
        return 1;
    }

    printf("\n========== RESULTADO ==========\n");
    printf("Total processado: %zu\n", total_processado);

    if (total_processado == 5 &&
        contexto.acessos_verificados == 5) {

        printf("TESTE APROVADO: arquivo lido corretamente!\n");
        return 0;
    }

    printf("TESTE FALHOU!\n");
    return 1;
}
