
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

    // 255 caracteres de conteudo + CR opcional + terminador NUL.
    // A leitura caractere a caractere tambem detecta bytes NUL no arquivo.
    char linha[258];
    size_t numero_linha = 0;

    while (true) {

        size_t tamanho = 0;
        bool linha_longa = false;
        bool byte_nulo = false;
        bool iniciou_linha = false;
        int caractere;

        // Le ate a quebra de linha ou o fim do arquivo sem estourar o buffer.
        while ((caractere = fgetc(arquivo)) != EOF &&
               caractere != '\n') {

            iniciou_linha = true;

            if (caractere == '\0') {
                byte_nulo = true;
            }

            if (tamanho < sizeof(linha) - 1) {
                linha[tamanho++] = (char)caractere;
            } else {
                linha_longa = true;
            }
        }

        // Nao ha nova linha a processar depois do ultimo EOF.
        if (!iniciou_linha && caractere == EOF) {
            break;
        }

        numero_linha++;

        if (ferror(arquivo)) {
            printf("ERRO: falha na leitura do arquivo.\n");
            fclose(arquivo);
            return false;
        }

        // Suporta terminacao CRLF, inclusive no limite de 255 caracteres.
        if (tamanho > 0 && linha[tamanho - 1] == '\r') {
            tamanho--;
        }

        if (linha_longa || tamanho > 255) {
            printf("ERRO: linha %zu muito longa.\n", numero_linha);
            fclose(arquivo);
            return false;
        }

        if (byte_nulo) {
            printf("ERRO: linha %zu contem byte NUL.\n", numero_linha);
            fclose(arquivo);
            return false;
        }

        linha[tamanho] = '\0';

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
