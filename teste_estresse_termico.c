#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <inttypes.h>

#include "substituicao.h"
#include "metricas.h"
#include "visualizacao.h"
#include "hardware_specs.h"

// Demonstra o Teste 2 da Prova de Fogo (Secao 6 da Especificacao):
// Preenche 100% da RAM fisica (256 KB = 64 blocos) e, no timestamp virtual 200,
// forca a leitura de 10 novas paginas consecutivas que nao estao na RAM.
int main(void) {
    printf("========== TESTE 2 DA PROVA DE FOGO: ESTRESSE TERMICO (WSCLOCK vs SWAP) ==========\n");

    // -------------------------------------------------------------------------
    // CENARIO 1: Substituicao Ineficiente (Codigo Ruim)
    // Expulsa 10 paginas modificadas (M=1) aleatoriamente no mesmo ms (t = 200)
    // -------------------------------------------------------------------------
    printf("\n--- CENARIO 1: ALGORITMO INEFICIENTE (10 Page Outs sujos em t=200ms) ---\n");

    hardware_state_t hw_ruim;
    hardware_init(&hw_ruim);

    Metricas metricas_ruim;
    inicializar_metricas(&metricas_ruim);

    for (int i = 1; i <= 10; i++) {
        hardware_state_t antes = hw_ruim;
        hardware_trigger_page_out(&hw_ruim, 200);
        registrar_operacao_swap(&metricas_ruim, &antes, &hw_ruim);

        uint64_t penalidade = hw_ruim.total_cpu_cycles - antes.total_cpu_cycles;
        printf("Page Out #%02d (t=200ms) | Temp Swap: %6.2f C | Custo: %10" PRIu64 " ciclos %s\n",
               i, hw_ruim.swap_temperature, penalidade,
               (hw_ruim.swap_temperature > MAX_SAFE_TEMP) ? "[THERMAL THROTTLING!]" : "");
    }

    printf("\nResumo Cenario Ineficiente:\n");
    printf("  Temperatura Maxima: %.2f C (Limite seguro: %.2f C)\n",
           metricas_ruim.temperatura_maxima, MAX_SAFE_TEMP);
    printf("  Total de Ciclos de E/S: %" PRIu64 " ciclos\n",
           metricas_ruim.ciclos_io);

    // -------------------------------------------------------------------------
    // CENARIO 2: WSClock Correto com Prioridade por Paginas Limpas (M=0)
    // RAM cheia (256 KB) com paginas sujas e limpas misturadas; em t=200ms
    // ocorrem 10 Page Faults consecutivos.
    // -------------------------------------------------------------------------
    printf("\n--- CENARIO 2: WSCLOCK CORRETO (Prioriza vitimas limpas M=0 em t=200ms) ---\n");

    hardware_state_t hw_wsclock;
    hardware_init(&hw_wsclock);

    Metricas metricas_wsclock;
    inicializar_metricas(&metricas_wsclock);

    BlocoRamVisual ram[TOTAL_BLOCOS_RAM] = {0};
    page_table_entry_t tabela[TOTAL_PAGINAS_VIRTUAIS] = {0};
    ProcessoMemoria processos[1] = {
        {1, tabela, TOTAL_PAGINAS_VIRTUAIS}
    };

    GerenciadorSubstituicao gerenciador;
    inicializar_substituicao(&gerenciador, WSCLOCK_TAU_PADRAO, true);

    // Preenche os 64 blocos da RAM (256 KB) usando 64 paginas pares (VPN 0, 2, ..., 126)
    // Metade modificada (M=1) e metade limpa (M=0)
    for (uint32_t i = 0; i < 64; i++) {
        uint32_t vpn = i * 2U;
        bool escrita = (i % 2U == 0U);
        if (!tratar_page_fault_wsclock(
                &gerenciador, &hw_wsclock, ram, TOTAL_BLOCOS_RAM,
                processos, 1, 1, vpn, escrita, 10 + i, 4096,
                &metricas_wsclock, false, NULL)) {
            printf("ERRO ao preencher a RAM inicial.\n");
            liberar_metricas(&metricas_ruim);
            liberar_metricas(&metricas_wsclock);
            return 1;
        }
    }

    // No timestamp 200ms, forca a leitura de 10 novas paginas consecutivas (VPN 128 a 146)
    for (uint32_t k = 0; k < 10; k++) {
        uint32_t nova_vpn = 128U + (k * 2U);
        ResultadoSubstituicao res = {0};

        if (!tratar_page_fault_wsclock(
                &gerenciador, &hw_wsclock, ram, TOTAL_BLOCOS_RAM,
                processos, 1, 1, nova_vpn, false, 200, 4096,
                &metricas_wsclock, false, &res)) {
            printf("ERRO ao tratar Page Fault no timestamp 200.\n");
            liberar_metricas(&metricas_ruim);
            liberar_metricas(&metricas_wsclock);
            return 1;
        }

        printf("Page Fault #%02u (t=200ms) -> Alocou VPN %3u no Frame %2u (Vitima: VPN %3u, Page Out: %s) | Temp: %.2f C\n",
               k + 1U, nova_vpn, res.frame_inicial, res.ultima_vpn_vitima,
               res.houve_page_out ? "SIM" : "NAO (M=0)",
               hw_wsclock.swap_temperature);
    }

    printf("\nResumo Cenario WSClock (Paginas Limpas Priorizadas):\n");
    printf("  Temperatura Maxima: %.2f C (Limite seguro: %.2f C)\n",
           metricas_wsclock.temperatura_maxima, MAX_SAFE_TEMP);
    printf("  Total de Ciclos de E/S: %" PRIu64 " ciclos\n",
           metricas_wsclock.ciclos_io);

    // -------------------------------------------------------------------------
    // CENARIO 3: Caso Extremo — 100% da RAM suja (M=1) + Escalonamento Termico
    // Mostra que mesmo se todas as 64 paginas da RAM forem sujas (M=1) e as
    // novas requisicoes tambem forem escritas (M=1), o escalonamento de
    // resfriamento passivo do WSClock impede o Thermal Throttling!
    // -------------------------------------------------------------------------
    printf("\n--- CENARIO 3: CASO EXTREMO (100%% da RAM suja M=1 com Resfriamento Passivo) ---\n");

    hardware_state_t hw_extremo;
    hardware_init(&hw_extremo);

    Metricas metricas_extremo;
    inicializar_metricas(&metricas_extremo);

    memset(ram, 0, sizeof(ram));
    memset(tabela, 0, sizeof(tabela));
    inicializar_substituicao(&gerenciador, WSCLOCK_TAU_PADRAO, true);

    for (uint32_t i = 0; i < 64; i++) {
        uint32_t vpn = i * 2U;
        tratar_page_fault_wsclock(
            &gerenciador, &hw_extremo, ram, TOTAL_BLOCOS_RAM,
            processos, 1, 1, vpn, true, 10 + i, 4096,
            &metricas_extremo, false, NULL
        );
    }

    for (uint32_t k = 0; k < 10; k++) {
        uint32_t nova_vpn = 128U + (k * 2U);
        ResultadoSubstituicao res = {0};

        tratar_page_fault_wsclock(
            &gerenciador, &hw_extremo, ram, TOTAL_BLOCOS_RAM,
            processos, 1, 1, nova_vpn, true, 200, 4096,
            &metricas_extremo, false, &res
        );

        printf("Page Fault Sujo #%02u -> Vitima VPN %3u escrita no Swap | Temp Swap: %.2f C | Ciclos E/S acumulados: %" PRIu64 "\n",
               k + 1U, res.ultima_vpn_vitima,
               hw_extremo.swap_temperature,
               metricas_extremo.ciclos_io);
    }

    bool aprovado =
        (metricas_ruim.temperatura_maxima > MAX_SAFE_TEMP) &&
        (metricas_ruim.ciclos_io > 16000ULL) &&
        (metricas_wsclock.temperatura_maxima <= MAX_SAFE_TEMP) &&
        (metricas_wsclock.ciclos_io == 0ULL) &&
        (metricas_extremo.temperatura_maxima <= MAX_SAFE_TEMP) &&
        (metricas_extremo.ciclos_io == 10000ULL);

    liberar_metricas(&metricas_ruim);
    liberar_metricas(&metricas_wsclock);
    liberar_metricas(&metricas_extremo);

    if (aprovado) {
        printf("\nTESTE DE ESTRESSE TERMICO APROVADO: Thermal Throttling evitado com sucesso!\n");
        return 0;
    }

    printf("\nTESTE DE ESTRESSE TERMICO FALHOU!\n");
    return 1;
}