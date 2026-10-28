#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <inttypes.h>

#include "substituicao.h"
#include "metricas.h"
#include "visualizacao.h"
#include "auditoria.h"
#include "hardware_specs.h"

int main(void) {
    int aprovados = 0;
    const int total_testes = 6;

    printf("========== TESTES DE SUBSTITUICAO (WSCLOCK) ==========\n");

    hardware_state_t hardware;
    hardware_init(&hardware);

    Metricas metricas;
    inicializar_metricas(&metricas);

    BlocoRamVisual ram[TOTAL_BLOCOS_RAM] = {0};
    page_table_entry_t tabela_p1[TOTAL_PAGINAS_VIRTUAIS] = {0};

    ProcessoMemoria processos[1] = {
        {1, tabela_p1, TOTAL_PAGINAS_VIRTUAIS}
    };

    GerenciadorSubstituicao gerenciador;
    inicializar_substituicao(&gerenciador, 20, true);

    // TESTE 1: Alocacao de pagina par (4 KB = 1 bloco) e pagina impar (8 KB = 2 blocos)
    printf("\nTESTE 1 - Alocacao assimetrica (4 KB e 8 KB):\n");
    ResultadoSubstituicao res1 = {0}, res2 = {0};

    bool ok1 = tratar_page_fault_wsclock(
        &gerenciador, &hardware, ram, TOTAL_BLOCOS_RAM,
        processos, 1, 1, 0, false, 10, 1024, &metricas, false, &res1
    );
    bool ok2 = tratar_page_fault_wsclock(
        &gerenciador, &hardware, ram, TOTAL_BLOCOS_RAM,
        processos, 1, 1, 1, true, 15, 5120, &metricas, false, &res2
    );

    if (ok1 && ok2 &&
        res1.frame_inicial == 0 && res1.blocos_alocados == 1 &&
        res2.frame_inicial == 1 && res2.blocos_alocados == 2 &&
        ram[1].bytes_utilizados == 4096 && ram[2].bytes_utilizados == 1024 &&
        tabela_p1[0].present && tabela_p1[1].present && tabela_p1[1].modified) {
        printf("TESTE 1 APROVADO: VPN 0 (1 bloco) e VPN 1 (2 blocos) mapeadas corretamente!\n");
        aprovados++;
    } else {
        printf("TESTE 1 FALHOU!\n");
    }

    // TESTE 2: Segunda chance do bit R e preferencia por vitima limpa (M=0)
    printf("\nTESTE 2 - Segunda chance (Bit R) e prioridade por vitima limpa (M=0):\n");
    hardware_init(&hardware);
    memset(ram, 0, sizeof(ram));
    memset(tabela_p1, 0, sizeof(tabela_p1));
    inicializar_substituicao(&gerenciador, 20, true);

    // Preenche toda a RAM (64 blocos) com 64 paginas pares (VPN 0, 2, 4, ..., 126)
    // Todas estao sujas (M=1), exceto a VPN 6 (Frame 3), que esta limpa (M=0) e com R=0!
    for (uint32_t i = 0; i < 64; i++) {
        uint32_t vpn = i * 2U;
        bool escrita = (i != 3U); // Frame 3 (VPN 6) eh limpa (M=0)
        tratar_page_fault_wsclock(
            &gerenciador, &hardware, ram, TOTAL_BLOCOS_RAM,
            processos, 1, 1, vpn, escrita, 10, 4096, &metricas, false, NULL
        );
    }

    // Simula que o Frame 3 ja teve seu bit R zerado
    tabela_p1[6].referenced = false;

    ResultadoSubstituicao res_evict = {0};
    bool ok_evict = tratar_page_fault_wsclock(
        &gerenciador, &hardware, ram, TOTAL_BLOCOS_RAM,
        processos, 1, 1, 128, false, 100, 4096, &metricas, false, &res_evict
    );

    if (ok_evict &&
        res_evict.houve_substituicao &&
        !res_evict.houve_page_out &&
        res_evict.frame_inicial == 3 &&
        res_evict.ultima_vpn_vitima == 6 &&
        !tabela_p1[6].present &&
        tabela_p1[128].present) {
        printf("TESTE 2 APROVADO: WSClock limpou bits R=1 e escolheu a vitima limpa (VPN 6, Frame 3) sem Page Out!\n");
        aprovados++;
    } else {
        printf("TESTE 2 FALHOU!\n");
    }

    // TESTE 3: Page Out obrigatorio quando todas as paginas estao sujas (M=1)
    printf("\nTESTE 3 - Despejo de pagina suja (Page Out no Swap):\n");
    hardware_init(&hardware);
    liberar_metricas(&metricas);
    inicializar_metricas(&metricas);
    memset(ram, 0, sizeof(ram));
    memset(tabela_p1, 0, sizeof(tabela_p1));
    inicializar_substituicao(&gerenciador, 20, true);

    for (uint32_t i = 0; i < 64; i++) {
        uint32_t vpn = i * 2U;
        tratar_page_fault_wsclock(
            &gerenciador, &hardware, ram, TOTAL_BLOCOS_RAM,
            processos, 1, 1, vpn, true, 10 + i, 4096, &metricas, false, NULL
        );
    }

    // A pagina mais antiga eh a VPN 0 (t=10)
    ResultadoSubstituicao res_dirty = {0};
    bool ok_dirty = tratar_page_fault_wsclock(
        &gerenciador, &hardware, ram, TOTAL_BLOCOS_RAM,
        processos, 1, 1, 128, false, 200, 4096, &metricas, false, &res_dirty
    );

    if (ok_dirty &&
        res_dirty.houve_substituicao &&
        res_dirty.houve_page_out &&
        res_dirty.ultima_vpn_vitima == 0 &&
        metricas.ciclos_io == 1000 &&
        hardware.swap_temperature == 37.0f) {
        printf("TESTE 3 APROVADO: vitima suja mais antiga (VPN 0) despejada com custo base de 1000 ciclos!\n");
        aprovados++;
    } else {
        printf("TESTE 3 FALHOU!\n");
    }

    // TESTE 4: Substituicao de 2 blocos contiguos para acomodar uma pagina impar de 8 KB
    printf("\nTESTE 4 - Substituicao contigua para pagina impar (8 KB):\n");
    ResultadoSubstituicao res_odd = {0};
    bool ok_odd = tratar_page_fault_wsclock(
        &gerenciador, &hardware, ram, TOTAL_BLOCOS_RAM,
        processos, 1, 1, 1, false, 250, 4096, &metricas, false, &res_odd
    );

    if (ok_odd &&
        res_odd.blocos_alocados == 2 &&
        res_odd.houve_substituicao &&
        tabela_p1[1].present &&
        ram[res_odd.frame_inicial].vpn == 1 &&
        ram[res_odd.frame_inicial + 1].vpn == 1 &&
        exibir_mapa_ram_ascii(ram, TOTAL_BLOCOS_RAM)) {
        printf("TESTE 4 APROVADO: pagina de 8 KB substituiu 2 blocos contiguos e passou na validacao visual!\n");
        aprovados++;
    } else {
        printf("TESTE 4 FALHOU!\n");
    }

    // TESTE 5: Rejeicao de VPN invalida (> 170) e excesso de bytes
    printf("\nTESTE 5 - Validacao de limites e parametros invalidos:\n");
    if (!tratar_page_fault_wsclock(
            &gerenciador, &hardware, ram, TOTAL_BLOCOS_RAM,
            processos, 1, 1, 171, false, 300, 4096, &metricas, false, NULL) &&
        !tratar_page_fault_wsclock(
            &gerenciador, &hardware, ram, TOTAL_BLOCOS_RAM,
            processos, 1, 1, 0, false, 300, 5000, &metricas, false, NULL)) {
        printf("TESTE 5 APROVADO: VPN 171 e tamanho excedido rejeitados com seguranca!\n");
        aprovados++;
    } else {
        printf("TESTE 5 FALHOU!\n");
    }

    // TESTE 6: Verificacao de ponteiros nulos
    printf("\nTESTE 6 - Rejeicao de ponteiros nulos:\n");
    if (!tratar_page_fault_wsclock(
            NULL, &hardware, ram, TOTAL_BLOCOS_RAM,
            processos, 1, 1, 0, false, 300, 1024, &metricas, false, NULL) &&
        !buscar_quadros_livres(NULL, TOTAL_BLOCOS_RAM, 1, &res1.frame_inicial)) {
        printf("TESTE 6 APROVADO: ponteiros nulos rejeitados!\n");
        aprovados++;
    } else {
        printf("TESTE 6 FALHOU!\n");
    }

    liberar_metricas(&metricas);

    printf("\n========== RESULTADO FINAL ==========\n");
    printf("Testes aprovados: %d/%d\n", aprovados, total_testes);

    if (aprovados == total_testes) {
        printf("TODOS OS TESTES DE SUBSTITUICAO FORAM APROVADOS!\n");
        return 0;
    }

    return 1;
}