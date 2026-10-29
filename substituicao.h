#ifndef SUBSTITUICAO_H
#define SUBSTITUICAO_H

#include "hardware_specs.h"
#include "visualizacao.h"
#include "metricas.h"
#include "auditoria.h"

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#define TOTAL_PAGINAS_VIRTUAIS 171
#define TOTAL_BLOCOS_RAM       (PHYSICAL_RAM_SIZE / PAGE_SIZE_EVEN) /* 64 blocos de 4 KB */
#define WSCLOCK_TAU_PADRAO     20U  /* Janela de conjunto de trabalho (Working Set) em ms */

// Registro de um processo gerenciado pelo subsistema de substituicao
typedef struct {
    uint32_t pid;
    page_table_entry_t *tabela;
    uint32_t quantidade_paginas;
} ProcessoMemoria;

// Estado do gerenciador de substituicao WSClock e controle termico do Swap
typedef struct {
    uint32_t ponteiro_relogio;
    uint32_t tau;
    uint32_t ultimo_timestamp_conhecido;
    bool resfriamento_preventivo;
    uint64_t substituicoes_limpas;
    uint64_t substituicoes_sujas;
} GerenciadorSubstituicao;

// Resultado detalhado de uma escolha de vitima ou alocacao
typedef struct {
    uint32_t frame_inicial;
    uint32_t blocos_alocados;
    bool houve_substituicao;
    bool houve_page_out;
    uint32_t vitimas_desalojadas;
    uint32_t ultimo_pid_vitima;
    uint32_t ultima_vpn_vitima;
} ResultadoSubstituicao;

// Inicializa o gerenciador WSClock e controle termico
void inicializar_substituicao(
    GerenciadorSubstituicao *gerenciador,
    uint32_t tau,
    bool resfriamento_preventivo
);

// Retorna a quantidade de blocos de 4 KB exigidos pela VPN (1 para par, 2 para impar)
uint32_t obter_blocos_necessarios(uint32_t vpn);

// Aplica o resfriamento passivo do Swap quando o tempo virtual avanca entre acessos
bool atualizar_resfriamento_swap(
    GerenciadorSubstituicao *gerenciador,
    hardware_state_t *hardware,
    uint32_t timestamp_atual
);

// Executa um Page Out controlado no Swap e registra o impacto termico e de ciclos em Metricas
bool executar_page_out_controlado(
    GerenciadorSubstituicao *gerenciador,
    hardware_state_t *hardware,
    Metricas *metricas,
    uint32_t timestamp_atual
);

// Procura quadros livres na RAM (1 bloco para 4 KB ou 2 blocos contiguos para 8 KB)
bool buscar_quadros_livres(
    const BlocoRamVisual *ram,
    size_t quantidade_blocos,
    uint32_t blocos_necessarios,
    uint32_t *frame_encontrado
);

// Trata um Page Fault: seleciona quadro livre ou aplica WSClock Termico, realiza Page Out se M=1,
// atualiza a RAM visual, a Tabela de Paginas e registra Auditoria e Metricas.
bool tratar_page_fault_wsclock(
    GerenciadorSubstituicao *gerenciador,
    hardware_state_t *hardware,
    BlocoRamVisual *ram,
    size_t quantidade_blocos,
    ProcessoMemoria *processos,
    size_t quantidade_processos,
    uint32_t pid_requisitante,
    uint32_t vpn_requisitada,
    bool operacao_escrita,
    uint32_t timestamp_atual,
    uint32_t bytes_utilizados_pagina,
    Metricas *metricas,
    bool exibir_auditoria,
    ResultadoSubstituicao *resultado
);

#endif