
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

#include "trace.h"
#include "auditoria.h"
#include "metricas.h"
#include "visualizacao.h"
#include "hardware_specs.h"

#define TOTAL_PAGINAS 171
#define TOTAL_BLOCOS 64

typedef struct {
    hardware_state_t hardware;
    page_table_entry_t tabelas[2][TOTAL_PAGINAS];
    BlocoRamVisual ram[TOTAL_BLOCOS];
    Metricas metricas;
    size_t mapas_exibidos;
} ContextoTeste;

// Processa cada acesso recebido do trace
bool processar_acesso_integrado(
    const AcessoMemoria *acesso,
    void *contexto
) {
    ContextoTeste *teste = contexto;

    if (acesso == NULL || teste == NULL) {
        return false;
    }

    if (acesso->pid < 1 || acesso->pid > 2) {
        return false;
    }

    // Cada processo possui sua propria tabela
    page_table_entry_t *tabela =
        teste->tabelas[acesso->pid - 1];

    page_table_entry_t antes[TOTAL_PAGINAS];

    memcpy(antes, tabela, sizeof(antes));

    // Realiza acesso pela MMU original
    bool hit = mmu_access_memory(
        &teste->hardware,
        tabela,
        TOTAL_PAGINAS,
        acesso->endereco,
        acesso->operacao == 'W',
        acesso->timestamp
    );

    // Registra acesso e possivel Page Fault
    if (!registrar_acesso(
            &teste->metricas,
            acesso->pid,
            !hit)) {
        return false;
    }

    printf("Acesso %llu | PID %u | %s\n",
           (unsigned long long)teste->metricas.total_acessos,
           acesso->pid,
           hit ? "HIT" : "PAGE FAULT");

    // Audita os eventos de Fading
    auditar_fading(
        antes,
        tabela,
        TOTAL_PAGINAS,
        acesso->pid,
        teste->hardware.global_access_count
    );

    // Exibe o mapa a cada 10 acessos
    if (teste->metricas.total_acessos % 10 == 0) {

        if (!exibir_mapa_ram_ascii(
                teste->ram, TOTAL_BLOCOS)) {
            return false;
        }

        // Registra fragmentacao da RAM ficticia
        if (!registrar_fragmentacao(
                &teste->metricas, 4096, 1024)) {
            return false;
        }

        teste->mapas_exibidos++;
    }

    return true;
}

int main(void) {

    ContextoTeste teste = {0};

    inicializar_metricas(&teste.metricas);
    hardware_init(&teste.hardware);

    srand(1234);

    // Pagina inicialmente presente do processo 1
    teste.tabelas[0][0].present = true;
    teste.tabelas[0][0].referenced = true;
    teste.tabelas[0][0].frame_number = 0;

    // Representacao ficticia da RAM
    teste.ram[0].ocupado = true;
    teste.ram[0].pid = 1;
    teste.ram[0].vpn = 0;
    teste.ram[0].bytes_utilizados = 1024;

    size_t total_processado = 0;

    printf("========== TESTE INTEGRADO ==========\n");

    // Processar os 20 acessos do trace
    bool sucesso = processar_arquivo_trace(
        "testes/trace_20.txt",
        processar_acesso_integrado,
        &teste,
        &total_processado
    );

    if (!sucesso) {
        printf("ERRO: processamento do trace.\n");
        liberar_metricas(&teste.metricas);
        return 1;
    }

    // Simular tres Page Outs para testar metricas
    for (int i = 0; i < 3; i++) {

        hardware_state_t antes = teste.hardware;

        hardware_trigger_page_out(
            &teste.hardware, 200
        );

        if (!registrar_operacao_swap(
                &teste.metricas,
                &antes,
                &teste.hardware)) {

            liberar_metricas(&teste.metricas);
            return 1;
        }
    }

    imprimir_metricas(&teste.metricas);

    // Verificar integracao dos modulos
    bool aprovado =
        total_processado == 20 &&
        teste.metricas.total_acessos == 20 &&
        teste.metricas.total_page_faults == 19 &&
        teste.hardware.global_access_count == 20 &&
        teste.mapas_exibidos == 2 &&
        teste.metricas.amostras_fragmentacao == 2 &&
        teste.metricas.temperatura_maxima == 67.0f &&
        teste.metricas.ciclos_io == 4000;

    liberar_metricas(&teste.metricas);

    if (aprovado) {
        printf("\nTESTE INTEGRADO APROVADO!\n");
        return 0;
    }

    printf("\nTESTE INTEGRADO FALHOU!\n");
    return 1;
}
