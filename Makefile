CC     ?= gcc
CFLAGS ?= -Wall -Wextra -std=c11 -g -I.

SRC = mmu.c ram.c hardware_specs.c
OBJ = $(SRC:%.c=build/%.o)
TESTES = build/teste_mmu build/teste_ram

.PHONY: test clean
.SECONDARY: $(OBJ)

test: $(TESTES)
	@for t in $(TESTES); do echo "== $$t"; ./$$t || exit 1; done

build/%.o: %.c | build
	$(CC) $(CFLAGS) -c -o $@ $<

build/teste_%: testes/teste_%.c testes/verificar.h $(OBJ) | build
	$(CC) $(CFLAGS) -o $@ $< $(OBJ)

build:
	mkdir -p build

clean:
	rm -rf build
