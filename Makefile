
# Antikit-8 - compilacao e testes integrados

.RECIPEPREFIX := >

CC = gcc
CFLAGS = -Wall -Wextra -Wpedantic -std=c11 -g -I.
LDLIBS = -lm

ifeq ($(OS),Windows_NT)
EXE = .exe
RUN = cmd /c
PREFIXO =
else
EXE =
RUN =
PREFIXO = ./
endif

FONTES = main.c mmu.c ram.c substituicao.c trace.c auditoria.c metricas.c visualizacao.c hardware_specs.c

TESTES = teste_mmu$(EXE) teste_ram$(EXE) teste_substituicao$(EXE) teste_estresse_termico$(EXE) teste_integracao$(EXE) teste_revisao$(EXE) teste_trace_extremos$(EXE)

.DEFAULT_GOAL := all
.PHONY: all test clean

all: antikit8$(EXE)

antikit8$(EXE): $(FONTES)
>$(CC) $(CFLAGS) $(FONTES) -o $@ $(LDLIBS)

teste_mmu$(EXE): testes/teste_mmu.c mmu.c ram.c hardware_specs.c
>$(CC) $(CFLAGS) $^ -o $@ $(LDLIBS)

teste_ram$(EXE): testes/teste_ram.c mmu.c ram.c hardware_specs.c
>$(CC) $(CFLAGS) $^ -o $@ $(LDLIBS)

teste_substituicao$(EXE): teste_substituicao.c substituicao.c mmu.c ram.c hardware_specs.c metricas.c auditoria.c visualizacao.c
>$(CC) $(CFLAGS) $^ -o $@ $(LDLIBS)

teste_estresse_termico$(EXE): teste_estresse_termico.c substituicao.c mmu.c ram.c hardware_specs.c metricas.c auditoria.c visualizacao.c
>$(CC) $(CFLAGS) $^ -o $@ $(LDLIBS)

teste_integracao$(EXE): testes/teste_integracao.c trace.c auditoria.c metricas.c visualizacao.c hardware_specs.c
>$(CC) $(CFLAGS) $^ -o $@ $(LDLIBS)

teste_revisao$(EXE): testes/teste_revisao.c auditoria.c trace.c visualizacao.c
>$(CC) $(CFLAGS) $^ -o $@ $(LDLIBS)

teste_trace_extremos$(EXE): testes/teste_trace_extremos.c trace.c
>$(CC) $(CFLAGS) $^ -o $@ $(LDLIBS)

test: $(TESTES)
>$(RUN) $(PREFIXO)teste_mmu$(EXE)
>$(RUN) $(PREFIXO)teste_ram$(EXE)
>$(RUN) $(PREFIXO)teste_substituicao$(EXE)
>$(RUN) $(PREFIXO)teste_estresse_termico$(EXE)
>$(RUN) $(PREFIXO)teste_integracao$(EXE)
>$(RUN) $(PREFIXO)teste_revisao$(EXE)
>$(RUN) $(PREFIXO)teste_trace_extremos$(EXE)

clean:
ifeq ($(OS),Windows_NT)
>-cmd /c del /q antikit8.exe teste_*.exe
else
>rm -f antikit8 teste_*
endif
