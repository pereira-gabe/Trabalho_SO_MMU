
#include <stdio.h>
#include <stdbool.h>
#include <stddef.h>

#include "trace.h"
#include "visualizacao.h"

#define TOTAL_BLOCOS 64

typedef struct {
    BlocoRamVisual ram[TOTAL_BLOCOS];
    size_t acessos;
    size_t mapas_exibidos;
} ContextoSimulacao;

// Funcao chamada a cada acesso do trace
bool processar_acesso(
    const AcessoMemoria *acesso,
    void *contexto
) {
    if (acesso == NULL || contexto == NULL) {
        return false;
    }

    ContextoSimulacao *simulacao = contexto;

    simulacao->acessos++;

    printf("Acesso %zu | PID %u | Operacao %c | Endereco %u\n",
           simulacao->acessos,
           acesso->pid,
           acesso->operacao,
           acesso->endereco);

    // Exibe o mapa a cada 10 acessos
    if (simulacao->acessos % 10 == 0) {

        printf("\n--- MAPA APOS %zu ACESSOS ---\n",
               simulacao->acessos);

        if (!exibir_mapa_ram_ascii(
                simulacao->ram, TOTAL_BLOCOS)) {
            return false;
        }

        simulacao->mapas_exibidos++;
    }

    return true;
}

int main(void) {

    ContextoSimulacao simulacao = {0};

    // Estado ficticio da RAM para testar a visualizacao
    simulacao.ram[0].ocupado = true;
    simulacao.ram[0].pid = 1;
    simulacao.ram[0].vpn = 0;
    simulacao.ram[0].bytes_utilizados = 1024;

    simulacao.ram[1].ocupado = true;
    simulacao.ram[1].pid = 1;
    simulacao.ram[1].vpn = 1;
    simulacao.ram[1].bytes_utilizados = 1024;

    simulacao.ram[2].ocupado = true;
    simulacao.ram[2].pid = 1;
    simulacao.ram[2].vpn = 1;
    simulacao.ram[2].bytes_utilizados = 0;

    size_t total_processado = 0;

    bool sucesso = processar_arquivo_trace(
        "testes/trace_20.txt",
        processar_acesso,
        &simulacao,
        &total_processado
    );

    if (!sucesso) {
        printf("TESTE FALHOU: erro no processamento.\n");
        return 1;
    }

    printf("\n========== RESULTADO FINAL ==========\n");
    printf("Acessos processados: %zu\n", total_processado);
    printf("Mapas exibidos: %zu\n", simulacao.mapas_exibidos);

    if (total_processado == 20 &&
        simulacao.acessos == 20 &&
        simulacao.mapas_exibidos == 2) {

        printf("TESTE APROVADO: exibicao a cada 10 acessos!\n");
        return 0;
    }

    printf("TESTE FALHOU!\n");
    return 1;
}
