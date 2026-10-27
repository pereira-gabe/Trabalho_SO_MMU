
#include "trace.h"
#include "hardware_specs.h"

#include <stdlib.h>
#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>

// Le um numero inteiro sem sinal
static bool ler_numero(const char **cursor, uint32_t *resultado) {

    // Ignora espacos em branco
    while (isspace((unsigned char)**cursor)) {
        (*cursor)++;
    }

    // Verifica se o proximo caractere e um digito
    if (!isdigit((unsigned char)**cursor)) {
        return false;
    }

    char *fim;
    errno = 0;

    unsigned long long numero = strtoull(*cursor, &fim, 10);

    // Verifica valores fora do intervalo permitido
    if (errno == ERANGE || numero > UINT32_MAX) {
        return false;
    }

    // Rejeita caracteres misturados ao numero
    if (*fim != '\0' && !isspace((unsigned char)*fim)) {
        return false;
    }

    *resultado = (uint32_t)numero;
    *cursor = fim;

    return true;
}

// Interpreta uma linha do arquivo de trace
bool interpretar_linha_trace(
    const char *linha,
    AcessoMemoria *acesso
) {
    if (linha == NULL || acesso == NULL) {
        return false;
    }

    AcessoMemoria temporario = {0};
    const char *cursor = linha;

    // Le o timestamp
    if (!ler_numero(&cursor, &temporario.timestamp)) {
        return false;
    }

    // Le o identificador do processo
    if (!ler_numero(&cursor, &temporario.pid)) {
        return false;
    }

    // Ignora espacos antes da operacao
    while (isspace((unsigned char)*cursor)) {
        cursor++;
    }

    // Verifica se a operacao e leitura ou escrita
    if (*cursor != 'R' && *cursor != 'W') {
        return false;
    }

    temporario.operacao = *cursor;
    cursor++;

    if (*cursor != '\0' &&
        !isspace((unsigned char)*cursor)) {
        return false;
    }

    // Le o endereco virtual
    if (!ler_numero(&cursor, &temporario.endereco)) {
        return false;
    }

    // Verifica o limite de 1 MB
    if (temporario.endereco >= VIRTUAL_MEMORY_SIZE) {
        return false;
    }

    // Ignora espacos ao final da linha
    while (isspace((unsigned char)*cursor)) {
        cursor++;
    }

    // Rejeita informacoes extras
    if (*cursor != '\0') {
        return false;
    }

    // Salva os dados apenas se toda a linha for valida
    *acesso = temporario;

    return true;
}

// Abre e processa o arquivo de trace linha por linha
bool processar_arquivo_trace(
    const char *caminho,
    ProcessadorTrace processar,
    void *contexto,
    size_t *total_processado
) {
    if (caminho == NULL || processar == NULL ||
        total_processado == NULL) {
        return false;
    }

    *total_processado = 0;

    FILE *arquivo = fopen(caminho, "r");

    if (arquivo == NULL) {
        printf("ERRO: nao foi possivel abrir %s\n", caminho);
        return false;
    }

    char linha[256];
    size_t numero_linha = 0;

    while (fgets(linha, sizeof(linha), arquivo) != NULL) {

        numero_linha++;

        // Detecta linhas maiores que o buffer
        size_t tamanho = strlen(linha);

        if (tamanho == sizeof(linha) - 1 &&
            linha[tamanho - 1] != '\n') {

            // Verifica se existem caracteres adicionais
            int proximo = fgetc(arquivo);

            if (proximo != EOF && proximo != '\n') {

                printf("ERRO: linha %zu muito longa.\n",
                       numero_linha);

                fclose(arquivo);
                return false;
            }
        }

        // Ignora linhas vazias
        const char *inicio = linha;

        while (isspace((unsigned char)*inicio)) {
            inicio++;
        }

        if (*inicio == '\0') {
            continue;
        }

        AcessoMemoria acesso;

        // Interpreta os dados da linha
        if (!interpretar_linha_trace(linha, &acesso)) {

            printf("ERRO: linha %zu invalida.\n",
                   numero_linha);

            fclose(arquivo);
            return false;
        }

        // Encaminha o acesso para processamento
        if (!processar(&acesso, contexto)) {

            printf("ERRO: falha ao processar linha %zu.\n",
                   numero_linha);

            fclose(arquivo);
            return false;
        }

        (*total_processado)++;
    }

    // Verifica se ocorreu algum erro de leitura
    if (ferror(arquivo)) {
        printf("ERRO: falha na leitura do arquivo.\n");
        fclose(arquivo);
        return false;
    }

    fclose(arquivo);
    return true;
}
