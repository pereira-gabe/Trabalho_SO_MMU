#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <inttypes.h>

#include "hardware_specs.h"
#include "trace.h"
#include "auditoria.h"
#include "metricas.h"
#include "visualizacao.h"
#include "substituicao.h"

#define MAX_PROCESSOS_SIMULADOS 16

typedef struct {
    hardware_state_t hardware;
    GerenciadorSubstituicao substituicao;
    Metricas metricas;
    BlocoRamVisual ram[TOTAL_BLOCOS_RAM];

    ProcessoMemoria processos[MAX_PROCESSOS_SIMULADOS];
    page_table_entry_t tabelas[MAX_PROCESSOS_SIMULADOS][TOTAL_PAGINAS_VIRTUAIS];
    size_t quantidade_processos;
} ContextoSimulador;

// Traduz um endereco virtual (0 a 1048575) em VPN e Offset respeitando a paginacao assimetrica
static bool traduzir_endereco_assimetrico(
    uint32_t endereco_virtual,
    uint32_t *vpn,
    uint32_t *offset
) {
    if (endereco_virtual >= VIRTUAL_MEMORY_SIZE ||
        vpn == NULL || offset == NULL) {
        return false;
    }

    uint32_t tamanho_par = PAGE_SIZE_EVEN + PAGE_SIZE_ODD; // 12288 bytes
    uint32_t indice_par = endereco_virtual / tamanho_par;
    uint32_t resto = endereco_virtual % tamanho_par;

    if (resto < PAGE_SIZE_EVEN) {
        *vpn = indice_par * 2U;
        *offset = resto;
    } else {
        *vpn = (indice_par * 2U) + 1U;
        *offset = resto - PAGE_SIZE_EVEN;
    }

    return (*vpn < TOTAL_PAGINAS_VIRTUAIS);
}

// Busca ou cadastra a tabela de paginas de um processo pelo PID
static ProcessoMemoria *obter_ou_criar_processo(
    ContextoSimulador *sim,
    uint32_t pid
) {
    for (size_t i = 0; i < sim->quantidade_processos; i++) {
        if (sim->processos[i].pid == pid) {
            return &sim->processos[i];
        }
    }

    if (sim->quantidade_processos >= MAX_PROCESSOS_SIMULADOS) {
        return NULL;
    }

    size_t idx = sim->quantidade_processos++;
    memset(sim->tabelas[idx], 0, sizeof(sim->tabelas[idx]));

    sim->processos[idx].pid = pid;
    sim->processos[idx].tabela = sim->tabelas[idx];
    sim->processos[idx].quantidade_paginas = TOTAL_PAGINAS_VIRTUAIS;

    return &sim->processos[idx];
}

// Callback invocado para cada linha valida lida por processar_arquivo_trace()
static bool processar_acesso_simulacao(
    const AcessoMemoria *acesso,
    void *contexto
) {
    ContextoSimulador *sim = contexto;
    if (acesso == NULL || sim == NULL) {
        return false;
    }

    uint32_t vpn = 0, offset = 0;
    if (!traduzir_endereco_assimetrico(acesso->endereco, &vpn, &offset)) {
        return false;
    }

    ProcessoMemoria *proc = obter_ou_criar_processo(sim, acesso->pid);
    if (proc == NULL) {
        return false;
    }

    // Salva copia da tabela antes do acesso para auditar possivel Fading do bit R
    page_table_entry_t antes[TOTAL_PAGINAS_VIRTUAIS];
    memcpy(antes, proc->tabela, sizeof(antes));

    bool escrita = (acesso->operacao == 'W');

    // Consulta a MMU fisica do hardware
    bool hit = mmu_access_memory(
        &sim->hardware,
        proc->tabela,
        TOTAL_PAGINAS_VIRTUAIS,
        acesso->endereco,
        escrita,
        acesso->timestamp
    );

    // Audita se houve Fading do bit R neste acesso global (multiplo de 5)
    auditar_fading(
        antes,
        proc->tabela,
        TOTAL_PAGINAS_VIRTUAIS,
        acesso->pid,
        sim->hardware.global_access_count
    );

    // Registra o acesso e se ocorreu Page Fault nas metricas
    if (!registrar_acesso(&sim->metricas, acesso->pid, !hit)) {
        return false;
    }

    // Estima bytes utilizados na pagina a partir do offset acessado (minimo 1024 B)
    uint32_t tamanho_pagina = (vpn % 2U == 0U) ? PAGE_SIZE_EVEN : PAGE_SIZE_ODD;
    uint32_t bytes_usados = offset + 1U;
    if (bytes_usados < 1024U) {
        bytes_usados = 1024U;
    }
    if (bytes_usados > tamanho_pagina) {
        bytes_usados = tamanho_pagina;
    }

    // Se ocorreu Page Fault, aciona o tratador WSClock da Parte 2
    if (!hit) {
        ResultadoSubstituicao resultado = {0};
        if (!tratar_page_fault_wsclock(
                &sim->substituicao,
                &sim->hardware,
                sim->ram,
                TOTAL_BLOCOS_RAM,
                sim->processos,
                sim->quantidade_processos,
                acesso->pid,
                vpn,
                escrita,
                acesso->timestamp,
                bytes_usados,
                &sim->metricas,
                true,
                &resultado)) {
            return false;
        }
    }

    // A cada 10 linhas processadas, exibe o Mapa ASCII e registra amostra de fragmentacao
    if (sim->metricas.total_acessos % 10ULL == 0ULL) {
        if (!exibir_mapa_ram_ascii(sim->ram, TOTAL_BLOCOS_RAM)) {
            return false;
        }

        uint32_t bytes_alocados = 0;
        uint32_t bytes_utilizados = 0;
        for (size_t i = 0; i < TOTAL_BLOCOS_RAM; i++) {
            if (sim->ram[i].ocupado) {
                bytes_alocados += PAGE_SIZE_EVEN;
                bytes_utilizados += sim->ram[i].bytes_utilizados;
            }
        }

        if (bytes_alocados > 0) {
            if (!registrar_fragmentacao(
                    &sim->metricas, bytes_alocados, bytes_utilizados)) {
                return false;
            }
        }
    }

    return true;
}

int main(int argc, char *argv[]) {
    const char *caminho_trace = (argc > 1) ? argv[1] : "memory_trace.txt";

    ContextoSimulador *sim = calloc(1, sizeof(ContextoSimulador));
    if (sim == NULL) {
        printf("ERRO: falha ao alocar memoria para o simulador.\n");
        return 1;
    }

    srand(1234);
    hardware_init(&sim->hardware);
    inicializar_metricas(&sim->metricas);
    inicializar_substituicao(&sim->substituicao, WSCLOCK_TAU_PADRAO, true);

    size_t total_processado = 0;

    if (!processar_arquivo_trace(
            caminho_trace,
            processar_acesso_simulacao,
            sim,
            &total_processado)) {
        liberar_metricas(&sim->metricas);
        free(sim);
        return 1;
    }

    imprimir_metricas(&sim->metricas);

    liberar_metricas(&sim->metricas);
    free(sim);
    return 0;
}