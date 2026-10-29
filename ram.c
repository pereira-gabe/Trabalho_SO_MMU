#include "ram.h"

#include <string.h>

void ram_inicializar(ram_t *ram) {
    if (ram != NULL) {
        memset(ram, 0, sizeof(*ram));
    }
}

bool ram_buscar_livre(const ram_t *ram, uint32_t blocos, uint32_t *frame) {
    if (ram == NULL || frame == NULL || (blocos != 1U && blocos != 2U)) {
        return false;
    }

    uint32_t livres_seguidos = 0;
    for (uint32_t i = 0; i < MMU_NUM_BLOCOS_RAM; i++) {
        livres_seguidos = ram->blocos[i].ocupado ? 0U : livres_seguidos + 1U;
        if (livres_seguidos == blocos) {
            *frame = i + 1U - blocos;
            return true;
        }
    }
    return false;
}

bool ram_alocar(ram_t *ram, uint32_t frame, uint32_t pid, uint32_t vpn,
                uint32_t bytes_utilizados) {
    if (ram == NULL || !mmu_vpn_valida(vpn)) {
        return false;
    }

    uint32_t blocos = mmu_blocos_da_pagina(vpn);
    if (bytes_utilizados == 0U || bytes_utilizados > mmu_tamanho_pagina(vpn) ||
        frame > MMU_NUM_BLOCOS_RAM - blocos) {
        return false;
    }

    for (uint32_t b = 0; b < blocos; b++) {
        if (ram->blocos[frame + b].ocupado) {
            return false;
        }
    }

    uint32_t restante = bytes_utilizados;
    for (uint32_t b = 0; b < blocos; b++) {
        uint32_t uso = (restante > MMU_BLOCO_BYTES) ? MMU_BLOCO_BYTES : restante;
        ram->blocos[frame + b] = (ram_bloco_t){true, pid, vpn, uso};
        restante -= uso;
    }
    return true;
}

/* Os dois blocos de uma pagina de 8 KB sao identicos em (pid, vpn); se o
 * anterior combina, frame e o segundo bloco. */
static uint32_t inicio_da_pagina(const ram_t *ram, uint32_t frame) {
    const ram_bloco_t *b = &ram->blocos[frame];

    if (frame > 0U && mmu_blocos_da_pagina(b->vpn) == 2U &&
        ram->blocos[frame - 1U].ocupado &&
        ram->blocos[frame - 1U].pid == b->pid &&
        ram->blocos[frame - 1U].vpn == b->vpn) {
        return frame - 1U;
    }
    return frame;
}

bool ram_liberar(ram_t *ram, uint32_t frame, uint32_t *pid, uint32_t *vpn) {
    if (ram == NULL || frame >= MMU_NUM_BLOCOS_RAM || !ram->blocos[frame].ocupado) {
        return false;
    }

    uint32_t inicio = inicio_da_pagina(ram, frame);
    uint32_t pid_liberado = ram->blocos[inicio].pid;
    uint32_t vpn_liberada = ram->blocos[inicio].vpn;

    for (uint32_t b = 0; b < mmu_blocos_da_pagina(vpn_liberada); b++) {
        ram->blocos[inicio + b] = (ram_bloco_t){0};
    }

    if (pid != NULL) {
        *pid = pid_liberado;
    }
    if (vpn != NULL) {
        *vpn = vpn_liberada;
    }
    return true;
}

uint32_t ram_blocos_ocupados(const ram_t *ram) {
    uint32_t total = 0;
    for (uint32_t i = 0; ram != NULL && i < MMU_NUM_BLOCOS_RAM; i++) {
        total += ram->blocos[i].ocupado ? 1U : 0U;
    }
    return total;
}

uint32_t ram_maior_livre_contiguo(const ram_t *ram) {
    uint32_t maior = 0;
    uint32_t atual = 0;

    for (uint32_t i = 0; ram != NULL && i < MMU_NUM_BLOCOS_RAM; i++) {
        atual = ram->blocos[i].ocupado ? 0U : atual + 1U;
        if (atual > maior) {
            maior = atual;
        }
    }
    return maior;
}

static const page_table_entry_t *tabela_do_pid(const ram_processo_t *processos,
                                               size_t quantidade, uint32_t pid) {
    for (size_t i = 0; i < quantidade; i++) {
        if (processos[i].pid == pid) {
            return processos[i].tabela;
        }
    }
    return NULL;
}

bool ram_validar(const ram_t *ram, const ram_processo_t *processos,
                 size_t quantidade, const char **motivo) {
    const char *erro = NULL;

    if (ram == NULL || (processos == NULL && quantidade > 0U)) {
        erro = "argumentos invalidos";
        goto falha;
    }

    for (uint32_t i = 0; i < MMU_NUM_BLOCOS_RAM; i++) {
        const ram_bloco_t *b = &ram->blocos[i];

        if (!b->ocupado) {
            if (b->bytes_utilizados != 0U) {
                erro = "bloco livre com bytes_utilizados residual";
                goto falha;
            }
            continue;
        }
        if (b->bytes_utilizados > MMU_BLOCO_BYTES) {
            erro = "bytes_utilizados maior que o bloco de 4 KB";
            goto falha;
        }

        const page_table_entry_t *tabela = tabela_do_pid(processos, quantidade, b->pid);
        if (tabela == NULL || !mmu_vpn_valida(b->vpn)) {
            erro = "bloco ocupado por PID sem tabela ou por VPN invalida";
            goto falha;
        }

        const page_table_entry_t *pte = &tabela[b->vpn];
        if (!pte->present) {
            erro = "bloco ocupado por pagina ausente na tabela";
            goto falha;
        }
        if (i < pte->frame_number ||
            i - pte->frame_number >= mmu_blocos_da_pagina(b->vpn)) {
            erro = "frame_number da PTE nao cobre o bloco ocupado";
            goto falha;
        }
    }

    for (size_t p = 0; p < quantidade; p++) {
        for (uint32_t vpn = 0; vpn < MMU_NUM_PAGINAS; vpn++) {
            const page_table_entry_t *pte = &processos[p].tabela[vpn];
            if (!pte->present) {
                continue;
            }

            uint32_t blocos = mmu_blocos_da_pagina(vpn);
            if (pte->frame_number > MMU_NUM_BLOCOS_RAM - blocos) {
                erro = "PTE presente com frame fora da RAM";
                goto falha;
            }

            for (uint32_t k = 0; k < blocos; k++) {
                const ram_bloco_t *b = &ram->blocos[pte->frame_number + k];
                if (!b->ocupado || b->pid != processos[p].pid || b->vpn != vpn) {
                    erro = "PTE presente aponta para bloco livre ou de outra pagina";
                    goto falha;
                }
            }
        }
    }
    return true;

falha:
    if (motivo != NULL) {
        *motivo = erro;
    }
    return false;
}
