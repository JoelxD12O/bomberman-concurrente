CC      = gcc
CFLAGS  = -Wall -Wextra -g -Iinclude -pthread
SRC     = $(wildcard src/*.c)
BIN     = bomberman

$(BIN): $(SRC) include/*.h
	$(CC) $(CFLAGS) -o $@ $(SRC)

# Detecta race conditions entre hilos (usar en la semana de integración)
tsan: $(SRC) include/*.h
	$(CC) $(CFLAGS) -fsanitize=thread -o $(BIN)_tsan $(SRC)

run: $(BIN)
	./$(BIN) tests/caso_ejemplo 10

# Pruebas unitarias (no corren el juego completo)
test: src/parser.c tests/unit/test_parser.c
	$(CC) $(CFLAGS) -o test_parser tests/unit/test_parser.c src/parser.c
	./test_parser

clean:
	rm -f $(BIN) $(BIN)_tsan test_parser

.PHONY: tsan run test clean
