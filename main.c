
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

    // Habilita pausa a cada 10 acessos
    bool pausar_mapas;
} ContextoSimulador;

// Traduz um endereco virtual considerando a paginacao assimetrica
static bool traduzir_endereco_assimetrico(
    uint32_t endereco_virtual,
    uint32_t *vpn,
    uint32_t *offset
) {
    if (endereco_virtual >= VIRTUAL_MEMORY_SIZE ||
        vpn == NULL || offset == NULL) {
        return false;
    }

    uint32_t tamanho_par = PAGE_SIZE_EVEN + PAGE_SIZE_ODD;
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

// Pausa a execucao para permitir inspecionar o mapa da RAM
static void pausar_para_inspecao(void) {
    printf("\n[SIMULADOR] Pressione ENTER para continuar...");
    fflush(stdout);

    int caractere;

    do {
        caractere = getchar();
    } while (caractere != '\n' && caractere != EOF);

    printf("\n");
}

// Processa cada acesso recebido do arquivo de trace
static bool processar_acesso_simulacao(
    const AcessoMemoria *acesso,
    void *contexto
) {
    ContextoSimulador *sim = contexto;

    if (acesso == NULL || sim == NULL) {
        return false;
    }

    uint32_t vpn = 0;
    uint32_t offset = 0;

    if (!traduzir_endereco_assimetrico(
            acesso->endereco, &vpn, &offset)) {
        return false;
    }

    ProcessoMemoria *proc =
        obter_ou_criar_processo(sim, acesso->pid);

    if (proc == NULL) {
        return false;
    }

    // Salva o estado anterior para auditar o Fading
    page_table_entry_t antes[TOTAL_PAGINAS_VIRTUAIS];

    memcpy(antes, proc->tabela, sizeof(antes));

    bool escrita = (acesso->operacao == 'W');

    // Consulta a MMU original do hardware
    bool hit = mmu_access_memory(
        &sim->hardware,
        proc->tabela,
        TOTAL_PAGINAS_VIRTUAIS,
        acesso->endereco,
        escrita,
        acesso->timestamp
    );

    // Audita possiveis eventos de Fading do bit R
    auditar_fading(
        antes,
        proc->tabela,
        TOTAL_PAGINAS_VIRTUAIS,
        acesso->pid,
        sim->hardware.global_access_count
    );

    // Registra acesso e possivel Page Fault
    if (!registrar_acesso(
            &sim->metricas,
            acesso->pid,
            !hit)) {
        return false;
    }

    // Estima a quantidade de bytes utilizados na pagina
    uint32_t tamanho_pagina =
        (vpn % 2U == 0U) ? PAGE_SIZE_EVEN : PAGE_SIZE_ODD;

    uint32_t bytes_usados = offset + 1U;

    if (bytes_usados < 1024U) {
        bytes_usados = 1024U;
    }

    if (bytes_usados > tamanho_pagina) {
        bytes_usados = tamanho_pagina;
    }

    // Trata Page Fault por meio do WSClock
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

    } else {
        // Atualiza a estimativa de utilizacao da pagina
        // quando ela ja esta presente na RAM

        size_t frame_inicial =
            proc->tabela[vpn].frame_number;

        size_t quantidade_frames =
            tamanho_pagina / PAGE_SIZE_EVEN;

        // Verifica os limites dos frames fisicos
        if (frame_inicial >= TOTAL_BLOCOS_RAM ||
            quantidade_frames > TOTAL_BLOCOS_RAM - frame_inicial) {

            printf("ERRO: mapeamento de RAM invalido.\n");
            return false;
        }

        for (size_t i = 0; i < quantidade_frames; i++) {

            BlocoRamVisual *bloco =
                &sim->ram[frame_inicial + i];

            // Confere a identidade da pagina
            if (!bloco->ocupado ||
                bloco->pid != acesso->pid ||
                bloco->vpn != vpn) {

                printf("ERRO: inconsistencia no mapa da RAM.\n");
                return false;
            }

            uint32_t inicio_bloco =
                (uint32_t)i * PAGE_SIZE_EVEN;

            uint32_t novos_bytes = 0;

            if (bytes_usados > inicio_bloco) {
                novos_bytes = bytes_usados - inicio_bloco;
            }

            if (novos_bytes > PAGE_SIZE_EVEN) {
                novos_bytes = PAGE_SIZE_EVEN;
            }

            // Nunca reduz os bytes ja contabilizados
            if (novos_bytes > bloco->bytes_utilizados) {
                bloco->bytes_utilizados = novos_bytes;
            }
        }
    }

    // Exibe o mapa e registra fragmentacao a cada 10 acessos
    if (sim->metricas.total_acessos % 10ULL == 0ULL) {

        if (!exibir_mapa_ram_ascii(
                sim->ram, TOTAL_BLOCOS_RAM)) {
            return false;
        }

        uint32_t bytes_alocados = 0;
        uint32_t bytes_utilizados = 0;

        for (size_t i = 0; i < TOTAL_BLOCOS_RAM; i++) {

            if (sim->ram[i].ocupado) {
                bytes_alocados += PAGE_SIZE_EVEN;
                bytes_utilizados +=
                    sim->ram[i].bytes_utilizados;
            }
        }

        if (bytes_alocados > 0) {
            if (!registrar_fragmentacao(
                    &sim->metricas,
                    bytes_alocados,
                    bytes_utilizados)) {
                return false;
            }
        }

        // Pausa opcional para inspecionar o mapa
        if (sim->pausar_mapas) {
            pausar_para_inspecao();
        }
    }

    return true;
}

int main(int argc, char *argv[]) {

    const char *caminho_trace = "memory_trace.txt";
    bool pausar_mapas = false;
    bool caminho_informado = false;

    // Interpreta os argumentos da linha de comando
    for (int i = 1; i < argc; i++) {

        if (strcmp(argv[i], "--pause") == 0) {

            if (pausar_mapas) {
                printf("ERRO: --pause informado mais de uma vez.\n");
                return 1;
            }

            pausar_mapas = true;

        } else if (!caminho_informado && argv[i][0] != '-') {

            caminho_trace = argv[i];
            caminho_informado = true;

        } else {

            printf("Uso: %s [arquivo_trace] [--pause]\n",
                   argv[0]);

            return 1;
        }
    }

    // Aloca o contexto do simulador
    ContextoSimulador *sim =
        calloc(1, sizeof(ContextoSimulador));

    if (sim == NULL) {
        printf("ERRO: falha ao alocar memoria para o simulador.\n");
        return 1;
    }

    // Define se a pausa esta habilitada
    sim->pausar_mapas = pausar_mapas;

    // Inicializa hardware, metricas e WSClock
    srand(1234);

    hardware_init(&sim->hardware);

    inicializar_metricas(&sim->metricas);

    inicializar_substituicao(
        &sim->substituicao,
        WSCLOCK_TAU_PADRAO,
        true
    );

    size_t total_processado = 0;

    // Processa o arquivo de trace
    if (!processar_arquivo_trace(
            caminho_trace,
            processar_acesso_simulacao,
            sim,
            &total_processado)) {

        liberar_metricas(&sim->metricas);
        free(sim);
        return 1;
    }

    // Exibe as estatisticas finais
    imprimir_metricas(&sim->metricas);

    // Libera os recursos alocados
    liberar_metricas(&sim->metricas);
    free(sim);

    return 0;
}
