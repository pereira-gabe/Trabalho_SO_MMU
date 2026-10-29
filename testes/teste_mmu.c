#include "mmu.h"
#include "verificar.h"

#include <string.h>

/* Mesma busca iterativa do motor de hardware, usada como oraculo. */
static bool traducao_iterativa(uint32_t endereco, uint32_t *vpn, uint32_t *offset) {
    uint32_t inicio = 0;

    for (uint32_t i = 0; i < MMU_NUM_PAGINAS; i++) {
        uint32_t tamanho = (i % 2 == 0) ? PAGE_SIZE_EVEN : PAGE_SIZE_ODD;
        if (endereco < inicio + tamanho) {
            *vpn = i;
            *offset = endereco - inicio;
            return true;
        }
        inicio += tamanho;
    }
    return false;
}

static void limites_da_especificacao(void) {
    const uint32_t enderecos[] = {12287, 12288, 1048575};
    const uint32_t vpn_esperada[] = {1, 2, 170};
    const uint32_t offset_esperado[] = {8191, 0, 4095};

    printf("Teste 1 - limites da traducao assimetrica:\n");
    for (int i = 0; i < 3; i++) {
        mmu_traducao_t t;
        VERIFICAR(mmu_traduzir(NULL, enderecos[i], &t), "traducao deve ser aceita");
        VERIFICAR(t.vpn == vpn_esperada[i] && t.offset == offset_esperado[i],
                  "VPN/Offset diferentes do esperado");
        mmu_imprimir_traducao(1, &t);
    }
    printf("\n");
}

static void constantes_e_bases(void) {
    uint32_t base;

    VERIFICAR(MMU_NUM_PAGINAS == 171 && MMU_ULTIMA_VPN == 170, "171 paginas, ultima VPN 170");
    VERIFICAR(MMU_NUM_BLOCOS_RAM == 64, "RAM de 256 KB = 64 blocos");
    VERIFICAR(mmu_base_virtual(1, &base) && base == 4096, "base da VPN 1");
    VERIFICAR(mmu_base_virtual(2, &base) && base == 12288, "base da VPN 2");
    VERIFICAR(mmu_base_virtual(170, &base) && base + mmu_tamanho_pagina(170) == VIRTUAL_MEMORY_SIZE,
              "a VPN 170 termina exatamente em 1 MB");
    VERIFICAR(!mmu_base_virtual(171, &base), "VPN 171 nao existe");
    VERIFICAR(mmu_blocos_da_pagina(4) == 1 && mmu_blocos_da_pagina(5) == 2,
              "VPN par usa 1 bloco, impar usa 2");
}

static void todos_os_enderecos(void) {
    bool igual_ao_oraculo = true;
    bool reconstroi_endereco = true;

    for (uint32_t a = 0; a < VIRTUAL_MEMORY_SIZE; a++) {
        uint32_t vpn, offset, vpn_ref, offset_ref, base;

        if (!mmu_traduzir_virtual(a, &vpn, &offset) ||
            !traducao_iterativa(a, &vpn_ref, &offset_ref)) {
            igual_ao_oraculo = false;
            break;
        }
        igual_ao_oraculo &= (vpn == vpn_ref && offset == offset_ref);
        reconstroi_endereco &= (mmu_base_virtual(vpn, &base) && base + offset == a &&
                                offset < mmu_tamanho_pagina(vpn));
    }

    VERIFICAR(igual_ao_oraculo, "resultado diferente da busca iterativa em algum endereco");
    VERIFICAR(reconstroi_endereco, "base + offset nao reconstroi o endereco");
}

static void entradas_invalidas(void) {
    uint32_t vpn = 77, offset = 88;

    VERIFICAR(!mmu_traduzir_virtual(VIRTUAL_MEMORY_SIZE, &vpn, &offset), "1 MB exato deve falhar");
    VERIFICAR(!mmu_traduzir_virtual(UINT32_MAX, &vpn, &offset), "UINT32_MAX deve falhar");
    VERIFICAR(vpn == 77 && offset == 88, "falha nao pode alterar as saidas");
    VERIFICAR(!mmu_traduzir_virtual(0, NULL, &offset) && !mmu_traduzir_virtual(0, &vpn, NULL),
              "ponteiros NULL devem falhar");
}

static void traducao_fisica(void) {
    page_table_entry_t tabela[MMU_NUM_PAGINAS];
    memset(tabela, 0, sizeof(tabela));
    uint32_t fisico;
    mmu_traducao_t t;

    VERIFICAR(!mmu_traduzir_fisico(tabela, 0, 0, &fisico), "pagina ausente nao tem endereco fisico");

    tabela[1] = (page_table_entry_t){.present = true, .frame_number = 10};
    VERIFICAR(mmu_traduzir_fisico(tabela, 1, 8191, &fisico) && fisico == 10 * 4096 + 8191,
              "pagina de 8 KB no frame 10, offset 8191");

    tabela[2] = (page_table_entry_t){.present = true, .frame_number = 63};
    VERIFICAR(mmu_traduzir_fisico(tabela, 2, 4095, &fisico) && fisico == PHYSICAL_RAM_SIZE - 1,
              "ultimo byte da RAM");
    VERIFICAR(!mmu_traduzir_fisico(tabela, 2, 4096, &fisico), "offset 4096 em pagina de 4 KB");

    tabela[3] = (page_table_entry_t){.present = true, .frame_number = 63};
    VERIFICAR(!mmu_traduzir_fisico(tabela, 3, 0, &fisico), "pagina de 8 KB no ultimo frame estoura a RAM");

    tabela[4] = (page_table_entry_t){.present = true, .frame_number = 64};
    VERIFICAR(!mmu_traduzir_fisico(tabela, 4, 0, &fisico), "frame fora da RAM");
    VERIFICAR(!mmu_traduzir(tabela, 2 * MMU_BYTES_PAR, &t), "PTE corrompida deve falhar em mmu_traduzir");

    VERIFICAR(mmu_traduzir(tabela, 12287, &t) && t.presente && t.vpn == 1 &&
              t.endereco_fisico == 10 * 4096 + 8191, "traducao completa de pagina presente");
    VERIFICAR(mmu_traduzir(tabela, 0, &t) && !t.presente, "traducao completa de pagina ausente");
    VERIFICAR(mmu_traduzir(NULL, 0, &t) && !t.presente, "tabela NULL equivale a pagina ausente");
}

/* O hit do hardware precisa ocorrer na mesma pagina que a traducao calcula. */
static void concordancia_com_hardware(void) {
    hardware_state_t hw;
    page_table_entry_t tabela[MMU_NUM_PAGINAS];
    bool concordam = true;

    hardware_init(&hw);

    for (uint32_t vpn = 0; vpn < MMU_NUM_PAGINAS; vpn++) {
        uint32_t base = 0;
        mmu_base_virtual(vpn, &base);
        uint32_t amostras[] = {base, base + mmu_tamanho_pagina(vpn) - 1U};

        for (int k = 0; k < 2; k++) {
            uint32_t v, o;
            mmu_traduzir_virtual(amostras[k], &v, &o);

            memset(tabela, 0, sizeof(tabela));
            tabela[v].present = true;
            concordam &= mmu_access_memory(&hw, tabela, MMU_NUM_PAGINAS, amostras[k], false, 0);
        }
    }
    VERIFICAR(concordam, "o hardware deve dar hit na VPN calculada");
}

int main(void) {
    limites_da_especificacao();
    constantes_e_bases();
    todos_os_enderecos();
    entradas_invalidas();
    traducao_fisica();
    concordancia_com_hardware();
    RESUMO();
}
