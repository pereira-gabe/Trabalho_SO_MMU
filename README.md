# Trabalho_SO_MMU

# Antikit-8 — Simulador de Gerenciamento de Memória

Trabalho prático desenvolvido para a disciplina de **Sistemas Operacionais**, com implementação em linguagem C.

O projeto simula o gerenciamento de memória de um hardware com restrições físicas, paginação assimétrica, substituição de páginas e controle térmico do Swap.

## 1. Funcionalidades implementadas

- **MMU:** tradução de endereços virtuais, identificação de páginas e offsets, respeitando páginas alternadas de 4 KB e 8 KB.
- **Gerenciamento da RAM:** alocação física de páginas, controle de frames e verificação de consistência.
- **WSClock:** substituição de páginas quando não há memória disponível, considerando os bits de referência (R), modificação (M) e informações de acesso.
- **Swap:** simulação do custo de Page Out e controle térmico para reduzir penalidades de desempenho.
- **Auditoria:** registro de alterações nas tabelas de páginas, checksums estruturais e de estado, além de monitoramento dos eventos de Fading.
- **Métricas:** contabilização de acessos, Page Faults globais e por processo, temperatura máxima do Swap, ciclos de E/S e fragmentação.
- **Visualização ASCII:** representação dos 64 frames físicos da RAM, com indicação de ocupação e fragmentação interna.

## 2. Especificações do hardware

| Característica               | Valor                              |
| ----------------------------- | ---------------------------------- |
| Memória virtual por processo | 1 MiB                              |
| RAM física                   | 256 KiB                            |
| Frames físicos               | 64 de 4 KiB                        |
| Páginas virtuais             | 171                                |
| Páginas pares                | 4 KiB                              |
| Páginas ímpares             | 8 KiB                              |
| Fading do bit R               | Tentativa a cada 5 acessos globais |
| Temperatura inicial do Swap   | 25 °C                             |
| Limite de penalidade térmica | Acima de 65 °C                    |

Os arquivos `hardware_specs.c` e `hardware_specs.h` correspondem ao hardware fornecido pelo professor e foram preservados.

## 3. Compilação e execução

O projeto utiliza GCC e GNU Make.

**Windows (MinGW/MSYS2):**

- Compilar: `mingw32-make all`
- Executar: `.\antikit8.exe .\testes\trace_hits.txt`
- Executar testes: `mingw32-make test`

**Linux:**

- Compilar: `make all`
- Executar: `./antikit8 testes/trace_hits.txt`
- Executar testes: `make test`

Também é possível fornecer outro arquivo de trace como argumento. Na ausência de argumento, o programa procura `memory_trace.txt` no diretório atual.

## 4. Formato dos arquivos de trace

Cada linha representa um acesso à memória no seguinte formato:

`TIMESTAMP PID OPERACAO ENDERECO`

Exemplo:

`0 1 R 0`

`1 1 W 4096`

`2 2 R 12287`

Campos:

- `TIMESTAMP`: instante simulado do acesso, em milissegundos.
- `PID`: identificador numérico do processo.
- `OPERACAO`: `R` para leitura ou `W` para escrita.
- `ENDERECO`: endereço virtual decimal, entre 0 e 1048575.

O simulador suporta até 16 processos distintos por execução.

Esse é o formato adotado na implementação e nos testes do grupo; sua correspondência com um eventual trace oficial da disciplina deve ser confirmada.

## 5. Testes e validação

O comando `mingw32-make test` ou `make test` executa os testes automatizados de MMU, RAM, WSClock, estresse térmico, integração controlada, revisão de código e validação do parser.

Também foram criados traces específicos para testar:

- Reutilização de páginas residentes e hits.
- Ocupação completa da RAM.
- Substituição de páginas e operações de Swap.
- Acessos repetidos após a RAM estar cheia.
- Fragmentação estimada em páginas de 4 KB e 8 KB.

Nos cenários executados durante a integração, o simulador processou 74 acessos, com 72 Page Faults e 2 hits. O teste de pressão registrou 8.000 ciclos de E/S e temperatura máxima de 60 °C.

Esses resultados correspondem aos traces sintéticos utilizados nos testes e não representam todas as cargas possíveis.

## 6. Organização e responsabilidades

**Integrante 1 — MMU e RAM**

Implementação da tradução de endereços, estruturas de gerenciamento da RAM e testes de consistência.

Arquivos principais: `mmu.c`, `mmu.h`, `ram.c`, `ram.h`.

Branch original: `feat/mmu`.

**Integrante 2 — WSClock e Swap**

Implementação do algoritmo de substituição, seleção de vítimas, Page Outs e mitigação do aquecimento do Swap.

Arquivos principais: `substituicao.c`, `substituicao.h`, testes de substituição e estresse térmico.

Branch original: `parte2-wsclock-swap`.

**Integrante 3 — Auditoria, métricas e visualização**

Implementação do parser de traces, auditoria das tabelas de páginas, monitoramento de Fading, registro de métricas e visualização ASCII da RAM.

Arquivos principais: `trace.c`, `auditoria.c`, `metricas.c`, `visualizacao.c`, respectivos cabeçalhos e testes.

Branch original: `feat/auditoria`.

A branch `feat/integracao` reúne as implementações e os ajustes realizados durante a integração.

O histórico de commits e os testes de cada módulo complementam a identificação das contribuições individuais.

## 7. Limitações da simulação

A utilização de memória apresentada no mapa ASCII é estimada a partir dos offsets acessados, considerando um mínimo inicial de 1.024 bytes por página. Assim, a fragmentação exibida é uma estimativa do modelo, não uma medição real do conteúdo armazenado.

A API do hardware simulado fornece o custo de Page Out, utilizado na contabilização dos ciclos de E/S.

O Fading realiza uma tentativa periódica de alteração do bit R, mas nem todo evento necessariamente produz uma mudança observável.

## 8. Referências

- Tanenbaum — *Sistemas Operacionais Modernos*, capítulo 3.
- Enunciado do Trabalho Prático Antikit-8.
- Arquivos `hardware_specs.c` e `hardware_specs.h` fornecidos na disciplina.


## Inspeção interativa da RAM

Para pausar a execução a cada 10 acessos e visualizar o mapa
ASCII da memória RAM, execute:

```powershell
.\antikit8.exe .\testes\trace_20.txt --pause
```
