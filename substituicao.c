#include "substituicao.h"

#include <stdio.h>
#include <string.h>
#include <inttypes.h>

// Verifica se a VPN inteira cabe no espaco virtual assimetrico de 1 MB (0 a 170)
static bool vpn_eh_valida(uint32_t vpn) {
    uint64_t inicio = (uint64_t)(vpn / 2U) *
                      (PAGE_SIZE_EVEN + PAGE_SIZE_ODD);

    if (vpn % 2U != 0U) {
        inicio += PAGE_SIZE_EVEN;
    }

    uint64_t tamanho = (vpn % 2U == 0U)
                       ? PAGE_SIZE_EVEN : PAGE_SIZE_ODD;

    return (inicio + tamanho) <= VIRTUAL_MEMORY_SIZE;
}

// Localiza o processo correspondente ao PID no vetor de processos registrados
static ProcessoMemoria *buscar_processo(
    ProcessoMemoria *processos,
    size_t quantidade_processos,
    uint32_t pid
) {
    if (processos == NULL) {
        return NULL;
    }

    for (size_t i = 0; i < quantidade_processos; i++) {
        if (processos[i].pid == pid &&
            processos[i].tabela != NULL) {
            return &processos[i];
        }
    }

    return NULL;
}

// Se o indice apontar para a segunda metade de uma pagina impar (8 KB),
// recua para o bloco inicial dessa pagina na RAM.
static uint32_t normalizar_inicio_pagina_ram(
    const BlocoRamVisual *ram,
    size_t quantidade_blocos,
    uint32_t frame_idx
) {
    if (ram == NULL || frame_idx == 0 ||
        frame_idx >= quantidade_blocos ||
        !ram[frame_idx].ocupado) {
        return frame_idx;
    }

    uint32_t vpn = ram[frame_idx].vpn;
    uint32_t pid = ram[frame_idx].pid;

    if (vpn % 2U != 0U &&
        ram[frame_idx - 1U].ocupado &&
        ram[frame_idx - 1U].pid == pid &&
        ram[frame_idx - 1U].vpn == vpn) {

        // Conta quantos blocos anteriores consecutivos tem o mesmo (pid, vpn)
        uint32_t recuo = 0;
        uint32_t pos = frame_idx;
        while (pos > 0 &&
               ram[pos - 1U].ocupado &&
               ram[pos - 1U].pid == pid &&
               ram[pos - 1U].vpn == vpn) {
            pos--;
            recuo++;
        }

        if (recuo % 2U == 1U) {
            return frame_idx - 1U;
        }
    }

    return frame_idx;
}

// Inicializa o gerenciador WSClock e controle termico
void inicializar_substituicao(
    GerenciadorSubstituicao *gerenciador,
    uint32_t tau,
    bool resfriamento_preventivo
) {
    if (gerenciador == NULL) {
        return;
    }

    *gerenciador = (GerenciadorSubstituicao){0};
    gerenciador->ponteiro_relogio = 0;
    gerenciador->tau = (tau == 0) ? WSCLOCK_TAU_PADRAO : tau;
    gerenciador->ultimo_timestamp_conhecido = 0;
    gerenciador->resfriamento_preventivo = resfriamento_preventivo;
}

// Retorna 1 bloco (4 KB) para VPN par ou 2 blocos (8 KB) para VPN impar
uint32_t obter_blocos_necessarios(uint32_t vpn) {
    return (vpn % 2U == 0U) ? 1U : 2U;
}

// Aplica o resfriamento passivo do Swap quando o tempo virtual avanca
bool atualizar_resfriamento_swap(
    GerenciadorSubstituicao *gerenciador,
    hardware_state_t *hardware,
    uint32_t timestamp_atual
) {
    if (gerenciador == NULL || hardware == NULL) {
        return false;
    }

    if (timestamp_atual > gerenciador->ultimo_timestamp_conhecido) {
        gerenciador->ultimo_timestamp_conhecido = timestamp_atual;
    }

    return true;
}

// Executa um Page Out controlado no Swap e registra em Metricas
bool executar_page_out_controlado(
    GerenciadorSubstituicao *gerenciador,
    hardware_state_t *hardware,
    Metricas *metricas,
    uint32_t timestamp_atual
) {
    if (gerenciador == NULL || hardware == NULL) {
        return false;
    }

    hardware_state_t estado_antes = *hardware;
    uint32_t timestamp_efetivo = timestamp_atual;

    // Se o modo de protecao termica estiver ativo e um novo Page Out fosse
    // ultrapassar MAX_SAFE_TEMP (65 C), aguarda o intervalo minimo de
    // resfriamento passivo (0.5 C por ms) antes da escrita no disco.
    if (gerenciador->resfriamento_preventivo) {
        float temp_projetada = hardware->swap_temperature;

        if (timestamp_efetivo == hardware->last_swap_ms) {
            temp_projetada += 15.0f;
        } else if (timestamp_efetivo > hardware->last_swap_ms) {
            uint32_t diff = timestamp_efetivo - hardware->last_swap_ms;
            float resfriada = hardware->swap_temperature - (diff * 0.5f);
            if (resfriada < BASE_SWAP_TEMP) {
                resfriada = BASE_SWAP_TEMP;
            }
            temp_projetada = resfriada + 12.0f;
        } else {
            temp_projetada += 15.0f;
        }

        if (temp_projetada > MAX_SAFE_TEMP) {
            // 26 ms de resfriamento passivo reduzem 13 C; somados a +12 C
            // do Page Out escalonado, mantem a temperatura estavel <= 65 C.
            uint32_t atraso_termico_ms = 26U;
            if (hardware->last_swap_ms + atraso_termico_ms > timestamp_efetivo) {
                timestamp_efetivo = hardware->last_swap_ms + atraso_termico_ms;
            }
        }
    }

    hardware_trigger_page_out(hardware, timestamp_efetivo);

    if (timestamp_efetivo > gerenciador->ultimo_timestamp_conhecido) {
        gerenciador->ultimo_timestamp_conhecido = timestamp_efetivo;
    }

    if (metricas != NULL) {
        if (!registrar_operacao_swap(metricas, &estado_antes, hardware)) {
            return false;
        }
    }

    return true;
}

// Procura quadros livres na RAM (1 bloco para pagina par, 2 blocos contiguos para impar)
bool buscar_quadros_livres(
    const BlocoRamVisual *ram,
    size_t quantidade_blocos,
    uint32_t blocos_necessarios,
    uint32_t *frame_encontrado
) {
    if (ram == NULL || frame_encontrado == NULL ||
        quantidade_blocos != TOTAL_BLOCOS_RAM ||
        (blocos_necessarios != 1U && blocos_necessarios != 2U)) {
        return false;
    }

    if (blocos_necessarios == 1U) {
        for (uint32_t i = 0; i < (uint32_t)quantidade_blocos; i++) {
            if (!ram[i].ocupado) {
                *frame_encontrado = i;
                return true;
            }
        }
    } else {
        for (uint32_t i = 0; i + 1U < (uint32_t)quantidade_blocos; i++) {
            if (!ram[i].ocupado && !ram[i + 1U].ocupado) {
                *frame_encontrado = i;
                return true;
            }
        }
    }

    return false;
}

// Desaloja a pagina que ocupa o bloco `frame_alvo` na RAM
static bool desalojar_pagina_no_frame(
    GerenciadorSubstituicao *gerenciador,
    hardware_state_t *hardware,
    BlocoRamVisual *ram,
    size_t quantidade_blocos,
    ProcessoMemoria *processos,
    size_t quantidade_processos,
    uint32_t frame_alvo,
    uint32_t timestamp_atual,
    Metricas *metricas,
    bool exibir_auditoria,
    ResultadoSubstituicao *resultado
) {
    if (frame_alvo >= quantidade_blocos || !ram[frame_alvo].ocupado) {
        return true;
    }

    uint32_t frame_cabeca = normalizar_inicio_pagina_ram(
        ram, quantidade_blocos, frame_alvo
    );

    uint32_t pid_vitima = ram[frame_cabeca].pid;
    uint32_t vpn_vitima = ram[frame_cabeca].vpn;
    uint32_t blocos_vitima = obter_blocos_necessarios(vpn_vitima);

    ProcessoMemoria *proc_vitima = buscar_processo(
        processos, quantidade_processos, pid_vitima
    );

    if (proc_vitima == NULL || vpn_vitima >= proc_vitima->quantidade_paginas) {
        return false;
    }

    page_table_entry_t *pte_vitima = &proc_vitima->tabela[vpn_vitima];

    // Verifica se a pagina vitima esta suja (modified == true)
    if (pte_vitima->present && pte_vitima->modified) {
        if (!executar_page_out_controlado(
                gerenciador, hardware, metricas, timestamp_atual)) {
            return false;
        }
        gerenciador->substituicoes_sujas++;
        if (resultado != NULL) {
            resultado->houve_page_out = true;
        }
    } else {
        gerenciador->substituicoes_limpas++;
    }

    // Limpa a entrada na Tabela de Paginas da vitima
    pte_vitima->present = false;
    pte_vitima->referenced = false;
    pte_vitima->modified = false;
    pte_vitima->frame_number = 0;

    // Libera o(s) bloco(s) correspondente(s) na RAM visual
    for (uint32_t b = 0;
         b < blocos_vitima && (frame_cabeca + b) < quantidade_blocos;
         b++) {
        ram[frame_cabeca + b].ocupado = false;
        ram[frame_cabeca + b].pid = 0;
        ram[frame_cabeca + b].vpn = 0;
        ram[frame_cabeca + b].bytes_utilizados = 0;
    }

    if (exibir_auditoria) {
        registrar_auditoria(
            proc_vitima->tabela,
            proc_vitima->quantidade_paginas,
            pid_vitima,
            "Pagina removida da RAM (WSClock)"
        );
    }

    if (resultado != NULL) {
        resultado->houve_substituicao = true;
        resultado->vitimas_desalojadas++;
        resultado->ultimo_pid_vitima = pid_vitima;
        resultado->ultima_vpn_vitima = vpn_vitima;
    }

    return true;
}

// Seleciona o frame candidato a substituicao segundo o algoritmo WSClock Termico:
//   Passagem 1: Examina o ponteiro circular. Se R == 1, da segunda chance (R = 0) e avanca.
//               Se R == 0, M == 0 (limpa) e idade > tau, escolhe imediatamente.
//   Passagem 2 (Prioridade Termica): Escolhe a pagina LIMPA (M == 0 e R == 0) mais antiga
//               para evitar Page Out e manter o Swap frio (< 65 C).
//   Passagem 3: Caso todas as paginas na RAM estejam sujas (M == 1), escolhe a pagina
//               com R == 0 de maior idade (menor last_access_t).
static bool selecionar_frame_wsclock(
    GerenciadorSubstituicao *gerenciador,
    const BlocoRamVisual *ram,
    size_t quantidade_blocos,
    ProcessoMemoria *processos,
    size_t quantidade_processos,
    uint32_t blocos_necessarios,
    uint32_t timestamp_atual,
    uint32_t *frame_escolhido
) {
    uint32_t limite_indice = (blocos_necessarios == 2U)
                             ? (uint32_t)(quantidade_blocos - 1U)
                             : (uint32_t)quantidade_blocos;

    // Passagem 1: Varredura do ponteiro do relogio (limpa bits R=1 e busca R=0, M=0, idade >= tau)
    for (uint32_t passo = 0; passo < (uint32_t)quantidade_blocos; passo++) {
        uint32_t idx = (gerenciador->ponteiro_relogio + passo) %
                       (uint32_t)quantidade_blocos;

        uint32_t cabeca = normalizar_inicio_pagina_ram(
            ram, quantidade_blocos, idx
        );

        // Processa apenas na cabeca de cada pagina para nao dar dupla penalidade a paginas de 8 KB
        if (cabeca != idx || idx >= limite_indice || !ram[idx].ocupado) {
            continue;
        }

        ProcessoMemoria *proc = buscar_processo(
            processos, quantidade_processos, ram[idx].pid
        );
        if (proc == NULL || ram[idx].vpn >= proc->quantidade_paginas) {
            continue;
        }

        page_table_entry_t *pte = &proc->tabela[ram[idx].vpn];

        if (pte->referenced) {
            // Segunda chance do WSClock: zera o bit R e avanca o ponteiro
            pte->referenced = false;
            continue;
        }

        uint32_t idade = (timestamp_atual >= pte->last_access_t)
                         ? (timestamp_atual - pte->last_access_t)
                         : 0U;

        if (!pte->modified && idade >= gerenciador->tau) {
            *frame_escolhido = idx;
            gerenciador->ponteiro_relogio =
                (idx + obter_blocos_necessarios(ram[idx].vpn)) %
                (uint32_t)quantidade_blocos;
            return true;
        }
    }

    // Passagem 2: Prioridade Termica — busca a pagina LIMPA (M == 0, R == 0) mais antiga
    // (Se for pagina de 8 KB precisando de 2 blocos, prefere par onde ambos sao limpos)
    bool encontrou_limpa = false;
    uint32_t melhor_frame_limpo = 0;
    uint32_t maior_idade_limpa = 0;
    bool melhor_par_totalmente_limpo = false;

    for (uint32_t passo = 0; passo < (uint32_t)quantidade_blocos; passo++) {
        uint32_t idx = (gerenciador->ponteiro_relogio + passo) %
                       (uint32_t)quantidade_blocos;

        uint32_t cabeca = normalizar_inicio_pagina_ram(
            ram, quantidade_blocos, idx
        );

        if (cabeca != idx || idx >= limite_indice || !ram[idx].ocupado) {
            continue;
        }

        ProcessoMemoria *proc = buscar_processo(
            processos, quantidade_processos, ram[idx].pid
        );
        if (proc == NULL || ram[idx].vpn >= proc->quantidade_paginas) {
            continue;
        }

        page_table_entry_t *pte = &proc->tabela[ram[idx].vpn];
        if (pte->referenced || pte->modified) {
            continue;
        }

        bool par_limpo = true;
        if (blocos_necessarios == 2U &&
            obter_blocos_necessarios(ram[idx].vpn) == 1U &&
            ram[idx + 1U].ocupado) {

            ProcessoMemoria *proc_viz = buscar_processo(
                processos, quantidade_processos, ram[idx + 1U].pid
            );
            if (proc_viz != NULL &&
                ram[idx + 1U].vpn < proc_viz->quantidade_paginas) {
                if (proc_viz->tabela[ram[idx + 1U].vpn].modified) {
                    par_limpo = false;
                }
            }
        }

        uint32_t idade = (timestamp_atual >= pte->last_access_t)
                         ? (timestamp_atual - pte->last_access_t)
                         : 0U;

        if (!encontrou_limpa ||
            (par_limpo && !melhor_par_totalmente_limpo) ||
            (par_limpo == melhor_par_totalmente_limpo && idade > maior_idade_limpa)) {
            encontrou_limpa = true;
            melhor_frame_limpo = idx;
            maior_idade_limpa = idade;
            melhor_par_totalmente_limpo = par_limpo;
        }
    }

    if (encontrou_limpa) {
        *frame_escolhido = melhor_frame_limpo;
        gerenciador->ponteiro_relogio =
            (melhor_frame_limpo + blocos_necessarios) %
            (uint32_t)quantidade_blocos;
        return true;
    }

    // Passagem 3: Todas as paginas candidatas estao sujas (M == 1).
    // Escolhe aquela com maior idade (menor last_access_t).
    bool encontrou_suja = false;
    uint32_t melhor_frame_sujo = 0;
    uint32_t maior_idade_suja = 0;

    for (uint32_t passo = 0; passo < (uint32_t)quantidade_blocos; passo++) {
        uint32_t idx = (gerenciador->ponteiro_relogio + passo) %
                       (uint32_t)quantidade_blocos;

        uint32_t cabeca = normalizar_inicio_pagina_ram(
            ram, quantidade_blocos, idx
        );

        if (cabeca != idx || idx >= limite_indice || !ram[idx].ocupado) {
            continue;
        }

        ProcessoMemoria *proc = buscar_processo(
            processos, quantidade_processos, ram[idx].pid
        );
        if (proc == NULL || ram[idx].vpn >= proc->quantidade_paginas) {
            continue;
        }

        page_table_entry_t *pte = &proc->tabela[ram[idx].vpn];
        uint32_t idade = (timestamp_atual >= pte->last_access_t)
                         ? (timestamp_atual - pte->last_access_t)
                         : 0U;

        if (!encontrou_suja || idade > maior_idade_suja) {
            encontrou_suja = true;
            melhor_frame_sujo = idx;
            maior_idade_suja = idade;
        }
    }

    if (encontrou_suja) {
        *frame_escolhido = melhor_frame_sujo;
        gerenciador->ponteiro_relogio =
            (melhor_frame_sujo + blocos_necessarios) %
            (uint32_t)quantidade_blocos;
        return true;
    }

    return false;
}

// Trata um Page Fault completo usando o WSClock Termico
bool tratar_page_fault_wsclock(
    GerenciadorSubstituicao *gerenciador,
    hardware_state_t *hardware,
    BlocoRamVisual *ram,
    size_t quantidade_blocos,
    ProcessoMemoria *processos,
    size_t quantidade_processos,
    uint32_t pid_requisitante,
    uint32_t vpn_requisitada,
    bool operacao_escrita,
    uint32_t timestamp_atual,
    uint32_t bytes_utilizados_pagina,
    Metricas *metricas,
    bool exibir_auditoria,
    ResultadoSubstituicao *resultado
) {
    if (gerenciador == NULL || hardware == NULL ||
        ram == NULL || processos == NULL ||
        quantidade_blocos != TOTAL_BLOCOS_RAM ||
        !vpn_eh_valida(vpn_requisitada)) {
        return false;
    }

    ProcessoMemoria *proc_req = buscar_processo(
        processos, quantidade_processos, pid_requisitante
    );
    if (proc_req == NULL ||
        vpn_requisitada >= proc_req->quantidade_paginas) {
        return false;
    }

    uint32_t tamanho_pagina = (vpn_requisitada % 2U == 0U)
                              ? PAGE_SIZE_EVEN : PAGE_SIZE_ODD;

    if (bytes_utilizados_pagina > tamanho_pagina) {
        return false;
    }

    // Se bytes_utilizados_pagina for 0, assume ocupacao completa da pagina
    uint32_t bytes_efetivos = (bytes_utilizados_pagina == 0U)
                              ? tamanho_pagina
                              : bytes_utilizados_pagina;

    ResultadoSubstituicao info = {0};
    uint32_t blocos_necessarios = obter_blocos_necessarios(vpn_requisitada);
    info.blocos_alocados = blocos_necessarios;

    atualizar_resfriamento_swap(gerenciador, hardware, timestamp_atual);

    uint32_t frame_destino = 0;

    // 1. Tenta encontrar quadro(s) livre(s) na RAM sem precisar expulsar ninguem
    if (!buscar_quadros_livres(
            ram, quantidade_blocos, blocos_necessarios, &frame_destino)) {

        // 2. RAM cheia: aciona o WSClock Termico para escolher o frame vitima
        if (!selecionar_frame_wsclock(
                gerenciador, ram, quantidade_blocos,
                processos, quantidade_processos,
                blocos_necessarios, timestamp_atual,
                &frame_destino)) {
            return false;
        }

        // Desaloja a pagina no primeiro bloco
        if (!desalojar_pagina_no_frame(
                gerenciador, hardware, ram, quantidade_blocos,
                processos, quantidade_processos,
                frame_destino, timestamp_atual,
                metricas, exibir_auditoria, &info)) {
            return false;
        }

        // Se a nova pagina for impar (8 KB = 2 blocos) e o segundo bloco ainda
        // estiver ocupado por outra pagina, desaloja tambem o segundo bloco.
        if (blocos_necessarios == 2U &&
            (frame_destino + 1U) < quantidade_blocos &&
            ram[frame_destino + 1U].ocupado) {

            if (!desalojar_pagina_no_frame(
                    gerenciador, hardware, ram, quantidade_blocos,
                    processos, quantidade_processos,
                    frame_destino + 1U, timestamp_atual,
                    metricas, exibir_auditoria, &info)) {
                return false;
            }
        }
    }

    info.frame_inicial = frame_destino;

    // 3. Preenche os blocos fisicos da RAM visual (respeitando o limite de 4 KB por bloco)
    for (uint32_t b = 0; b < blocos_necessarios; b++) {
        uint32_t idx = frame_destino + b;
        ram[idx].ocupado = true;
        ram[idx].pid = pid_requisitante;
        ram[idx].vpn = vpn_requisitada;

        if (blocos_necessarios == 1U) {
            ram[idx].bytes_utilizados = bytes_efetivos;
        } else {
            if (b == 0U) {
                ram[idx].bytes_utilizados =
                    (bytes_efetivos > PAGE_SIZE_EVEN)
                    ? PAGE_SIZE_EVEN : bytes_efetivos;
            } else {
                ram[idx].bytes_utilizados =
                    (bytes_efetivos > PAGE_SIZE_EVEN)
                    ? (bytes_efetivos - PAGE_SIZE_EVEN) : 0U;
            }
        }
    }

    // 4. Atualiza a Entrada da Tabela de Paginas (PTE) do processo requisitante
    page_table_entry_t *pte_nova = &proc_req->tabela[vpn_requisitada];
    pte_nova->frame_number = frame_destino;
    pte_nova->present = true;
    pte_nova->referenced = true;
    pte_nova->modified = operacao_escrita;
    pte_nova->last_access_t = timestamp_atual;

    if (exibir_auditoria) {
        registrar_auditoria(
            proc_req->tabela,
            proc_req->quantidade_paginas,
            pid_requisitante,
            "Pagina mapeada na RAM (WSClock)"
        );
    }

    if (resultado != NULL) {
        *resultado = info;
    }

    return true;
}