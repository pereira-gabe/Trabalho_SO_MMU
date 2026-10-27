
#include <stdio.h>
#include <inttypes.h>
#include "metricas.h"

int main(void) {

    Metricas metricas;
    inicializar_metricas(&metricas);

    // Simulando acessos de tres processos
    bool sucesso = true;

    sucesso = sucesso && registrar_acesso(&metricas, 1, false);
    sucesso = sucesso && registrar_acesso(&metricas, 1, true);
    sucesso = sucesso && registrar_acesso(&metricas, 2, true);
    sucesso = sucesso && registrar_acesso(&metricas, 1, true);
    sucesso = sucesso && registrar_acesso(&metricas, 2, false);
    sucesso = sucesso && registrar_acesso(&metricas, 3, false);

    if (!sucesso) {
        printf("ERRO: falha ao registrar acesso.\n");
        liberar_metricas(&metricas);
        return 1;
    }

    imprimir_metricas(&metricas);

    // TESTE 1: Total de acessos
    if (metricas.total_acessos == 6) {
        printf("\nTESTE 1 APROVADO: total de acessos correto!\n");
    } else {
        printf("\nTESTE 1 FALHOU!\n");
        liberar_metricas(&metricas);
        return 1;
    }

    // TESTE 2: Page Faults globais
    if (metricas.total_page_faults == 3) {
        printf("TESTE 2 APROVADO: Page Faults globais corretos!\n");
    } else {
        printf("TESTE 2 FALHOU!\n");
        liberar_metricas(&metricas);
        return 1;
    }

    // TESTE 3: Quantidade de processos
    if (metricas.quantidade_processos == 3) {
        printf("TESTE 3 APROVADO: tres processos registrados!\n");
    } else {
        printf("TESTE 3 FALHOU!\n");
        liberar_metricas(&metricas);
        return 1;
    }

    // TESTE 4: Page Faults por processo
    if (metricas.processos[0].page_faults == 2 &&
        metricas.processos[1].page_faults == 1 &&
        metricas.processos[2].page_faults == 0) {

        printf("TESTE 4 APROVADO: Page Faults por processo corretos!\n");
    } else {
        printf("TESTE 4 FALHOU!\n");
        liberar_metricas(&metricas);
        return 1;
    }

    // TESTE 5: Temperatura e ciclos de E/S
    printf("\n--- TESTE 5: SWAP TERMICO ---\n");

    hardware_state_t hardware;
    hardware_init(&hardware);

    // Simular tres Page Outs no timestamp 200
    for (int i = 1; i <= 3; i++) {

        hardware_state_t estado_antes = hardware;

        hardware_trigger_page_out(&hardware, 200);

        if (!registrar_operacao_swap(
                &metricas, &estado_antes, &hardware)) {

            printf("TESTE 5 FALHOU: erro no registro!\n");
            liberar_metricas(&metricas);
            return 1;
        }

        uint64_t custo =
            hardware.total_cpu_cycles -
            estado_antes.total_cpu_cycles;

        printf("Page Out %d: Temperatura = %.2f C, "
               "Custo = %" PRIu64 " ciclos\n",
               i, hardware.swap_temperature, custo);
    }

    // Verificar resultados finais do Swap
    if (metricas.temperatura_maxima == 67.0f &&
        metricas.ciclos_io == 4000) {

        printf("TESTE 5 APROVADO: temperatura e ciclos corretos!\n");

    } else {
        printf("TESTE 5 FALHOU!\n");
        liberar_metricas(&metricas);
        return 1;
    }

    // Exibir estatisticas atualizadas
    printf("\n--- RELATORIO FINAL ATUALIZADO ---\n");
    imprimir_metricas(&metricas);

    // TESTE 6: Fragmentacao interna media
printf("\n--- TESTE 6: FRAGMENTACAO INTERNA ---\n");

bool frag_ok = true;

frag_ok = frag_ok && registrar_fragmentacao(&metricas, 4096, 1024);
frag_ok = frag_ok && registrar_fragmentacao(&metricas, 8192, 1024);
frag_ok = frag_ok && registrar_fragmentacao(&metricas, 8192, 4096);

if (!frag_ok) {
    printf("TESTE 6 FALHOU: erro ao registrar amostras!\n");
    liberar_metricas(&metricas);
    return 1;
}

double media = metricas.soma_fragmentacao /
              metricas.amostras_fragmentacao;

printf("Fragmentacao media: %.2f%%\n", media);

if (media > 70.82 && media < 70.84) {
    printf("TESTE 6 APROVADO!\n");
} else {
    printf("TESTE 6 FALHOU!\n");
    liberar_metricas(&metricas);
    return 1;
}

imprimir_metricas(&metricas);

    // Liberar memoria alocada
    liberar_metricas(&metricas);

    printf("\nTODOS OS TESTES FORAM APROVADOS!\n");

    return 0;
}
