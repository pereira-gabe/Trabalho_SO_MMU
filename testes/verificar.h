#ifndef VERIFICAR_H
#define VERIFICAR_H

#include <stdio.h>

static int verificacoes_total = 0;
static int verificacoes_falhas = 0;

#define VERIFICAR(cond, descricao) do { \
    verificacoes_total++; \
    if (!(cond)) { \
        verificacoes_falhas++; \
        printf("FALHA (linha %d): %s\n", __LINE__, descricao); \
    } \
} while (0)

#define RESUMO() do { \
    printf("%d/%d verificacoes passaram\n", \
           verificacoes_total - verificacoes_falhas, verificacoes_total); \
    return verificacoes_falhas == 0 ? 0 : 1; \
} while (0)

#endif
