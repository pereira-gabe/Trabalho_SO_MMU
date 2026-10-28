#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>

#include "auditoria.h"
#include "trace.h"
#include "visualizacao.h"

static bool contar_acessos(const AcessoMemoria *acesso, void *contexto) {
    if (acesso == NULL || contexto == NULL) {
        return false;
    }
    (*(size_t *)contexto)++;
    return true;
}

int main(void) {
    int aprovados = 0;
    const int total_testes = 5;

    page_table_entry_t tabela[1] = {0};
    uint64_t checksum_estrutural = calcular_checksum(tabela, 1);
    uint64_t checksum_estado = calcular_checksum_estado(tabela, 1);

    tabela[0].referenced = true;
    tabela[0].modified = true;
    tabela[0].last_access_t = 42;

    if (calcular_checksum(tabela, 1) == checksum_estrutural &&
        calcular_checksum_estado(tabela, 1) != checksum_estado) {
        puts("TESTE 1 APROVADO: checksum de estado detecta R/M/tempo.");
        aprovados++;
    } else {
        puts("TESTE 1 FALHOU!");
    }

    FILE *arquivo = fopen("testes/temp_revisao_crlf.txt", "wb");
    if (arquivo == NULL) {
        return 1;
    }

    char linha[255];
    memset(linha, ' ', sizeof(linha));
    memcpy(linha, "1 1 R 0", 7);
    fwrite(linha, 1, sizeof(linha), arquivo);
    fputs("\r\n2 1 W 4\r\n", arquivo);
    fclose(arquivo);

    size_t recebidos = 0, processados = 0;
    bool sucesso = processar_arquivo_trace(
        "testes/temp_revisao_crlf.txt", contar_acessos,
        &recebidos, &processados
    );
    remove("testes/temp_revisao_crlf.txt");

    if (sucesso && recebidos == 2 && processados == 2) {
        puts("TESTE 2 APROVADO: limite de 255 com CRLF.");
        aprovados++;
    } else {
        puts("TESTE 2 FALHOU!");
    }

    arquivo = fopen("testes/temp_revisao_nul.txt", "wb");
    if (arquivo == NULL) {
        return 1;
    }
    const unsigned char linha_nula[] = {
        '1', ' ', '1', ' ', 'R', ' ', '0', '\0', 'X', '\n'
    };
    fwrite(linha_nula, 1, sizeof(linha_nula), arquivo);
    fclose(arquivo);

    recebidos = 0;
    processados = 0;
    sucesso = processar_arquivo_trace(
        "testes/temp_revisao_nul.txt", contar_acessos,
        &recebidos, &processados
    );
    remove("testes/temp_revisao_nul.txt");

    if (!sucesso && recebidos == 0 && processados == 0) {
        puts("TESTE 3 APROVADO: byte NUL rejeitado.");
        aprovados++;
    } else {
        puts("TESTE 3 FALHOU!");
    }

    double frag = 0;
    if (!calcular_fragmentacao_pagina(UINT32_MAX, 0, &frag) &&
        calcular_fragmentacao_pagina(170, 0, &frag)) {
        puts("TESTE 4 APROVADO: VPN invalida rejeitada, ultima valida aceita.");
        aprovados++;
    } else {
        puts("TESTE 4 FALHOU!");
    }

    BlocoRamVisual ram[64] = {0};
    ram[1].ocupado = true;
    ram[1].pid = 1;
    ram[1].vpn = UINT32_MAX;
    ram[1].bytes_utilizados = 100;

    if (!exibir_mapa_ram_ascii(ram, 64)) {
        puts("TESTE 5 APROVADO: mapa com VPN invalida rejeitado.");
        aprovados++;
    } else {
        puts("TESTE 5 FALHOU!");
    }

    printf("\nTESTES DA REVISAO: %d/%d aprovados.\n",
           aprovados, total_testes);

    return aprovados == total_testes ? 0 : 1;
}
