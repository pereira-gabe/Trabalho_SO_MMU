#ifndef TRACE_H
#define TRACE_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

typedef struct {
    uint32_t timestamp;
    uint32_t pid;
    char operacao;
    uint32_t endereco;
} AcessoMemoria;

bool interpretar_linha_trace(
    const char *linha,
    AcessoMemoria *acesso
);

typedef bool (*ProcessadorTrace)(
    const AcessoMemoria *acesso,
    void *contexto
);

bool processar_arquivo_trace(
    const char *caminho,
    ProcessadorTrace processar,
    void *contexto,
    size_t *total_processado
);

#endif