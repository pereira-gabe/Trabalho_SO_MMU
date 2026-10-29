#include "mmu.h"

#include <inttypes.h>
#include <stdio.h>

uint32_t mmu_tamanho_pagina(uint32_t vpn) {
    return (vpn % 2U == 0U) ? PAGE_SIZE_EVEN : PAGE_SIZE_ODD;
}

uint32_t mmu_blocos_da_pagina(uint32_t vpn) {
    return mmu_tamanho_pagina(vpn) / MMU_BLOCO_BYTES;
}

bool mmu_vpn_valida(uint32_t vpn) {
    return vpn < MMU_NUM_PAGINAS;
}

bool mmu_base_virtual(uint32_t vpn, uint32_t *base) {
    if (base == NULL || !mmu_vpn_valida(vpn)) {
        return false;
    }

    uint32_t inicio = (vpn / 2U) * MMU_BYTES_PAR;
    if (vpn % 2U != 0U) {
        inicio += PAGE_SIZE_EVEN;
    }

    *base = inicio;
    return true;
}

bool mmu_traduzir_virtual(uint32_t endereco_virtual, uint32_t *vpn, uint32_t *offset) {
    if (vpn == NULL || offset == NULL || endereco_virtual >= VIRTUAL_MEMORY_SIZE) {
        return false;
    }

    /* O resto da divisao por 12 KB diz se o endereco cai na pagina de 4 KB
     * (resto < 4096) ou na de 8 KB que a segue. */
    uint32_t par = endereco_virtual / MMU_BYTES_PAR;
    uint32_t resto = endereco_virtual % MMU_BYTES_PAR;

    if (resto < PAGE_SIZE_EVEN) {
        *vpn = par * 2U;
        *offset = resto;
    } else {
        *vpn = par * 2U + 1U;
        *offset = resto - PAGE_SIZE_EVEN;
    }

    return true;
}

bool mmu_traduzir_fisico(const page_table_entry_t *tabela, uint32_t vpn,
                         uint32_t offset, uint32_t *endereco_fisico) {
    if (tabela == NULL || endereco_fisico == NULL || !mmu_vpn_valida(vpn) ||
        offset >= mmu_tamanho_pagina(vpn) || !tabela[vpn].present) {
        return false;
    }

    uint32_t frame = tabela[vpn].frame_number;
    if (frame > MMU_NUM_BLOCOS_RAM - mmu_blocos_da_pagina(vpn)) {
        return false;
    }

    *endereco_fisico = frame * MMU_BLOCO_BYTES + offset;
    return true;
}

bool mmu_traduzir(const page_table_entry_t *tabela, uint32_t endereco_virtual,
                  mmu_traducao_t *resultado) {
    mmu_traducao_t t = {0};

    if (resultado == NULL ||
        !mmu_traduzir_virtual(endereco_virtual, &t.vpn, &t.offset)) {
        return false;
    }

    t.endereco_virtual = endereco_virtual;
    t.tamanho_pagina = mmu_tamanho_pagina(t.vpn);
    t.presente = (tabela != NULL && tabela[t.vpn].present);

    if (t.presente &&
        !mmu_traduzir_fisico(tabela, t.vpn, t.offset, &t.endereco_fisico)) {
        return false;
    }

    *resultado = t;
    return true;
}

void mmu_imprimir_traducao(uint32_t pid, const mmu_traducao_t *t) {
    if (t == NULL) {
        return;
    }

    const char *marca = "";
    if (t->offset == 0U) {
        marca = " (inicio da pagina)";
    } else if (t->offset == t->tamanho_pagina - 1U) {
        marca = " (fim da pagina)";
    }

    printf("[MMU] PID %" PRIu32 " | virtual %" PRIu32 " -> VPN %" PRIu32
           " (%" PRIu32 " KB), Offset %" PRIu32 "%s",
           pid, t->endereco_virtual, t->vpn, t->tamanho_pagina / 1024U,
           t->offset, marca);

    if (t->presente) {
        printf(" | fisico %" PRIu32 "\n", t->endereco_fisico);
    } else {
        printf(" | fora da RAM\n");
    }
}
