#include "ram.h"
#include "verificar.h"

#include <string.h>

static void alocacao_e_liberacao(void) {
    ram_t ram;
    uint32_t frame, pid, vpn;

    ram_inicializar(&ram);
    VERIFICAR(ram_blocos_ocupados(&ram) == 0 && ram_maior_livre_contiguo(&ram) == 64,
              "RAM recem-inicializada esta toda livre");

    VERIFICAR(ram_alocar(&ram, 0, 1, 0, 1024), "pagina de 4 KB no frame 0");
    VERIFICAR(ram_alocar(&ram, 1, 1, 1, 5 * 1024), "pagina de 8 KB nos frames 1 e 2");
    VERIFICAR(ram.blocos[1].bytes_utilizados == 4096 && ram.blocos[2].bytes_utilizados == 1024,
              "5 KB de uso devem ser repartidos em 4096 + 1024");
    VERIFICAR(ram.blocos[1].vpn == 1 && ram.blocos[2].vpn == 1 &&
              ram.blocos[1].pid == 1 && ram.blocos[2].pid == 1,
              "os dois blocos guardam o mesmo (pid, vpn)");

    VERIFICAR(!ram_alocar(&ram, 2, 2, 3, 100), "nao sobrescreve bloco ocupado");
    VERIFICAR(ram.blocos[2].pid == 1, "alocacao recusada nao altera a RAM");
    VERIFICAR(!ram_alocar(&ram, 63, 2, 1, 100), "pagina de 8 KB no ultimo bloco estoura a RAM");
    VERIFICAR(!ram_alocar(&ram, 10, 2, 2, 0) && !ram_alocar(&ram, 10, 2, 2, 4097),
              "bytes_utilizados fora de 1..tamanho da pagina");
    VERIFICAR(!ram_alocar(&ram, 10, 2, 171, 100), "VPN inexistente");

    VERIFICAR(ram_liberar(&ram, 2, &pid, &vpn) && pid == 1 && vpn == 1,
              "liberar pelo segundo bloco identifica a pagina");
    VERIFICAR(!ram.blocos[1].ocupado && !ram.blocos[2].ocupado &&
              ram.blocos[1].bytes_utilizados == 0 && ram.blocos[2].bytes_utilizados == 0,
              "os dois blocos voltam ao estado livre");
    VERIFICAR(ram.blocos[0].ocupado, "pagina vizinha permanece");
    VERIFICAR(!ram_liberar(&ram, 2, NULL, NULL), "liberar bloco livre falha");

    VERIFICAR(ram_buscar_livre(&ram, 2, &frame) && frame == 1, "o espaco liberado e reaproveitado");
}

static void fragmentacao_externa(void) {
    ram_t ram;
    uint32_t frame;

    ram_inicializar(&ram);
    for (uint32_t i = 0; i < MMU_NUM_BLOCOS_RAM; i += 2) {
        ram_alocar(&ram, i, 1, i, 4096);
    }

    VERIFICAR(ram_blocos_ocupados(&ram) == 32 && ram_maior_livre_contiguo(&ram) == 1,
              "metade livre, mas sem dois blocos contiguos");
    VERIFICAR(ram_buscar_livre(&ram, 1, &frame) && frame == 1, "pagina de 4 KB cabe");
    VERIFICAR(!ram_buscar_livre(&ram, 2, &frame), "pagina de 8 KB nao cabe");
    VERIFICAR(!ram_buscar_livre(&ram, 3, &frame), "pedido de 3 blocos e invalido");

    ram_liberar(&ram, 2, NULL, NULL);
    VERIFICAR(ram_maior_livre_contiguo(&ram) == 3 && ram_buscar_livre(&ram, 2, &frame) && frame == 1,
              "liberar um bloco junta os vizinhos livres");
}

static void deteccao_de_inconsistencias(void) {
    ram_t ram;
    page_table_entry_t tabela[MMU_NUM_PAGINAS];
    ram_processo_t proc = {7, tabela};
    const char *motivo = NULL;

    ram_inicializar(&ram);
    memset(tabela, 0, sizeof(tabela));
    VERIFICAR(ram_validar(&ram, &proc, 1, &motivo), "estado vazio e consistente");

    ram_alocar(&ram, 4, 7, 5, 8192);
    tabela[5] = (page_table_entry_t){.present = true, .frame_number = 4};
    VERIFICAR(ram_validar(&ram, &proc, 1, &motivo), "mapeamento correto e consistente");

    tabela[5].frame_number = 5;
    VERIFICAR(!ram_validar(&ram, &proc, 1, &motivo) && motivo != NULL, "frame_number deslocado");
    tabela[5].frame_number = 4;

    tabela[5].present = false;
    VERIFICAR(!ram_validar(&ram, &proc, 1, &motivo), "bloco ocupado por pagina ausente");
    tabela[5].present = true;

    tabela[2] = (page_table_entry_t){.present = true, .frame_number = 20};
    VERIFICAR(!ram_validar(&ram, &proc, 1, &motivo), "PTE presente apontando para bloco livre");
    tabela[2].present = false;

    ram.blocos[9].bytes_utilizados = 100;
    VERIFICAR(!ram_validar(&ram, &proc, 1, &motivo), "bloco livre com bytes residuais");
    ram.blocos[9].bytes_utilizados = 0;

    ram.blocos[4].pid = 99;
    VERIFICAR(!ram_validar(&ram, &proc, 1, &motivo), "bloco de PID sem tabela");
    ram.blocos[4].pid = 7;

    VERIFICAR(ram_validar(&ram, &proc, 1, &motivo), "volta a ser consistente ao corrigir");
}

/* Troca de paginas com o hardware: a cada Page Fault, expulsa vitimas (em
 * rodizio) ate caber a pagina nova. RAM e tabelas devem concordar o tempo todo. */
static void estresse_com_hardware(void) {
    enum { PROCESSOS = 3, ACESSOS = 30000 };
    static page_table_entry_t tabelas[PROCESSOS][MMU_NUM_PAGINAS];
    static ram_t ram;
    ram_processo_t procs[PROCESSOS];
    hardware_state_t hw;
    const char *motivo = NULL;
    uint32_t semente = 12345U, cursor = 0, faults = 0, tempo = 0;
    bool consistente = true, fisico_valido = true;

    hardware_init(&hw);
    ram_inicializar(&ram);
    for (uint32_t p = 0; p < PROCESSOS; p++) {
        procs[p] = (ram_processo_t){p + 1U, tabelas[p]};
    }

    for (int i = 0; i < ACESSOS && consistente; i++) {
        semente = semente * 1664525U + 1013904223U;
        uint32_t p = (semente >> 8) % PROCESSOS;
        uint32_t endereco = (semente >> 4) % VIRTUAL_MEMORY_SIZE;
        bool escrita = (semente >> 3) & 1U;
        tempo += (i % 4 == 0);

        uint32_t vpn, offset;
        mmu_traduzir_virtual(endereco, &vpn, &offset);

        if (!mmu_access_memory(&hw, tabelas[p], MMU_NUM_PAGINAS, endereco, escrita, tempo)) {
            faults++;
            uint32_t frame;
            while (!ram_buscar_livre(&ram, mmu_blocos_da_pagina(vpn), &frame)) {
                while (!ram.blocos[cursor].ocupado) {
                    cursor = (cursor + 1U) % MMU_NUM_BLOCOS_RAM;
                }
                uint32_t pid_vitima, vpn_vitima;
                ram_liberar(&ram, cursor, &pid_vitima, &vpn_vitima);
                tabelas[pid_vitima - 1U][vpn_vitima].present = false;
            }
            ram_alocar(&ram, frame, procs[p].pid, vpn, offset + 1U);
            tabelas[p][vpn] = (page_table_entry_t){frame, true, true, escrita, tempo};
        }

        mmu_traducao_t t;
        fisico_valido &= mmu_traduzir(tabelas[p], endereco, &t) && t.presente &&
                         t.endereco_fisico < PHYSICAL_RAM_SIZE;
        consistente = ram_validar(&ram, procs, PROCESSOS, &motivo);
    }

    VERIFICAR(faults > MMU_NUM_BLOCOS_RAM, "a RAM deve encher e forcar substituicoes");
    VERIFICAR(consistente, motivo != NULL ? motivo : "RAM e tabelas devem concordar a cada acesso");
    VERIFICAR(fisico_valido, "toda pagina acessada tem endereco fisico dentro dos 256 KB");
}

int main(void) {
    alocacao_e_liberacao();
    fragmentacao_externa();
    deteccao_de_inconsistencias();
    estresse_com_hardware();
    RESUMO();
}
