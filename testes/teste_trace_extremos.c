
#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include "trace.h"

// Conta quantos acessos foram recebidos
bool contar_acesso_extremo(
    const AcessoMemoria *acesso,
    void *contexto
) {
    if (acesso == NULL || contexto == NULL) {
        return false;
    }

    size_t *contador = contexto;
    (*contador)++;

    return true;
}

int main(void) {

    int aprovados = 0;
    size_t total;
    size_t contador;
    bool resultado;
    FILE *arquivo;

    printf("========== TESTES EXTREMOS DO TRACE ==========\n");

    // TESTE 1: Arquivo vazio
    arquivo = fopen("testes/temp_vazio.txt", "w");

    if (arquivo == NULL) {
        return 1;
    }

    fclose(arquivo);

    total = 0;
    contador = 0;

    resultado = processar_arquivo_trace(
        "testes/temp_vazio.txt",
        contar_acesso_extremo,
        &contador,
        &total
    );

    if (resultado && total == 0 && contador == 0) {
        printf("TESTE 1 APROVADO: arquivo vazio!\n");
        aprovados++;
    } else {
        printf("TESTE 1 FALHOU!\n");
    }

    remove("testes/temp_vazio.txt");

    // TESTE 2: Ultima linha sem quebra de linha
    arquivo = fopen("testes/temp_sem_quebra.txt", "w");

    if (arquivo == NULL) {
        return 1;
    }

    fprintf(arquivo, "10 1 R 4096");
    fclose(arquivo);

    total = 0;
    contador = 0;

    resultado = processar_arquivo_trace(
        "testes/temp_sem_quebra.txt",
        contar_acesso_extremo,
        &contador,
        &total
    );

    if (resultado && total == 1 && contador == 1) {
        printf("TESTE 2 APROVADO: arquivo sem quebra final!\n");
        aprovados++;
    } else {
        printf("TESTE 2 FALHOU!\n");
    }

    remove("testes/temp_sem_quebra.txt");

    // TESTE 3: Linha valida com exatamente 255 caracteres
    arquivo = fopen("testes/temp_limite.txt", "w");

    if (arquivo == NULL) {
        return 1;
    }

    char linha[256];

    memset(linha, ' ', 255);
    memcpy(linha, "10 1 R 4096", 11);
    linha[255] = '\0';

    fwrite(linha, 1, 255, arquivo);
    fclose(arquivo);

    total = 0;
    contador = 0;

    resultado = processar_arquivo_trace(
        "testes/temp_limite.txt",
        contar_acesso_extremo,
        &contador,
        &total
    );

    if (resultado && total == 1 && contador == 1) {
        printf("TESTE 3 APROVADO: limite de 255 caracteres!\n");
        aprovados++;
    } else {
        printf("TESTE 3 FALHOU!\n");
    }

    remove("testes/temp_limite.txt");

    // TESTE 4: Linha maior que o buffer
    arquivo = fopen("testes/temp_longo.txt", "w");

    if (arquivo == NULL) {
        return 1;
    }

    char linha_longa[300];

    memset(linha_longa, ' ', sizeof(linha_longa));
    memcpy(linha_longa, "10 1 R 4096", 11);

    fwrite(linha_longa, 1, sizeof(linha_longa), arquivo);
    fclose(arquivo);

    total = 0;
    contador = 0;

    resultado = processar_arquivo_trace(
        "testes/temp_longo.txt",
        contar_acesso_extremo,
        &contador,
        &total
    );

    if (!resultado && total == 0 && contador == 0) {
        printf("TESTE 4 APROVADO: linha longa rejeitada!\n");
        aprovados++;
    } else {
        printf("TESTE 4 FALHOU!\n");
    }

    remove("testes/temp_longo.txt");

    printf("\n========== RESULTADO ==========\n");
    printf("Testes aprovados: %d/4\n", aprovados);

    return (aprovados == 4) ? 0 : 1;
}
