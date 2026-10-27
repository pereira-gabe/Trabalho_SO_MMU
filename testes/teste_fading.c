
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <inttypes.h>

#include "auditoria.h"
#include "hardware_specs.h"

int main(void) {

    printf("========== TESTE DO FADING ==========\n");

    hardware_state_t hardware;
    hardware_init(&hardware);

    // Tabela ficticia com duas paginas
    page_table_entry_t tabela[2] = {0};

    // Prever a pagina sorteada usando a mesma semente
    srand(1234);
    uint32_t vpn_sorteada = (uint32_t)(rand() % 2);

    // Reiniciar a sequencia aleatoria para a MMU
    srand(1234);

    // Manter a pagina sorteada presente e referenciada
    tabela[vpn_sorteada].present = true;
    tabela[vpn_sorteada].referenced = true;
    tabela[vpn_sorteada].frame_number = 3;

    // Acessar a outra pagina, que esta ausente
    uint32_t endereco =
        (vpn_sorteada == 0) ? PAGE_SIZE_EVEN : 0;

    printf("VPN monitorada: %" PRIu32 "\n", vpn_sorteada);

    // Realizar quatro acessos sem atingir o Fading
    for (int i = 0; i < 4; i++) {

        bool hit = mmu_access_memory(
            &hardware, tabela, 2,
            endereco, false, 100 + i
        );

        if (hit) {
            printf("TESTE FALHOU: acesso inesperado.\n");
            return 1;
        }
    }

    if (!tabela[vpn_sorteada].referenced ||
        hardware.global_access_count != 4) {
        printf("TESTE FALHOU: estado inicial incorreto.\n");
        return 1;
    }

    printf("\nQuatro acessos realizados.\n");
    printf("Bit R antes do Fading: %d\n",
           tabela[vpn_sorteada].referenced);

    // Copiar o estado antes do quinto acesso
    page_table_entry_t antes[2] = {
        tabela[0], tabela[1]
    };

    uint64_t checksum_antes =
        calcular_checksum(tabela, 2);

    // Quinto acesso: o hardware executa o Fading
    mmu_access_memory(
        &hardware, tabela, 2,
        endereco, false, 104
    );

    printf("\nQuinto acesso realizado.\n");
    printf("Bit R depois do Fading: %d\n",
           tabela[vpn_sorteada].referenced);

    // Executar nossa auditoria
    auditar_fading(
        antes, tabela, 2, 1,
        hardware.global_access_count
    );

    uint64_t checksum_depois =
        calcular_checksum(tabela, 2);

    // Verificar o resultado
    if (hardware.global_access_count == 5 &&
        !tabela[vpn_sorteada].referenced &&
        checksum_antes == checksum_depois) {

        printf("\nTESTE APROVADO: Fading detectado!\n");
        printf("Checksum estrutural permaneceu consistente.\n");

    } else {
        printf("\nTESTE FALHOU!\n");
        return 1;
    }

    
    // TESTE 2: Fading sem alteracao observavel
    printf("\n--- TESTE 2: FADING SEM ALTERACAO ---\n");

    hardware_state_t hardware2;
    hardware_init(&hardware2);

    // Todas as paginas estao ausentes da RAM
    page_table_entry_t tabela2[2] = {0};

    // Realizar quatro acessos
    for (int i = 0; i < 4; i++) {
        mmu_access_memory(
            &hardware2, tabela2, 2,
            0, false, 200 + i
        );
    }

    // Salvar a tabela antes do quinto acesso
    page_table_entry_t antes2[2] = {
        tabela2[0], tabela2[1]
    };

    uint64_t checksum_antes2 =
        calcular_checksum(tabela2, 2);

    // Quinto acesso: evento de Fading
    mmu_access_memory(
        &hardware2, tabela2, 2,
        0, false, 204
    );

    auditar_fading(
        antes2, tabela2, 2, 1,
        hardware2.global_access_count
    );

    uint64_t checksum_depois2 =
        calcular_checksum(tabela2, 2);

    if (hardware2.global_access_count == 5 &&
        !tabela2[0].referenced &&
        !tabela2[1].referenced &&
        checksum_antes2 == checksum_depois2) {

        printf("TESTE 2 APROVADO: evento sem alteracao observavel!\n");

    } else {
        printf("TESTE 2 FALHOU!\n");
        return 1;
    }

    printf("\nTODOS OS TESTES DE FADING FORAM APROVADOS!\n");


    return 0;
}
