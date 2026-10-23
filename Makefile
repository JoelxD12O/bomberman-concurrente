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

clean:
	rm -f $(BIN) $(BIN)_tsan

.PHONY: tsan run clean
