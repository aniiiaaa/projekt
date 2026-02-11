CC=gcc
CFLAGS=-Wall -Wextra -O2 -std=c11 -Iinclude

ALL=main pasazer kasa kierowca dyspozytor logger
OBJ_COMMON=src/synch.o src/processes.o src/log_ipc.o

all: $(ALL)

main: src/main.o $(OBJ_COMMON)
	$(CC) $(CFLAGS) -o $@ $^

pasazer: src/pasazer_main.o $(OBJ_COMMON)
	$(CC) $(CFLAGS) -o $@ $^

kasa: src/kasa_main.o $(OBJ_COMMON)
	$(CC) $(CFLAGS) -o $@ $^

kierowca: src/kierowca_main.o $(OBJ_COMMON)
	$(CC) $(CFLAGS) -o $@ $^

dyspozytor: src/dyspozytor_main.o $(OBJ_COMMON)
	$(CC) $(CFLAGS) -o $@ $^

# logger potrzebuje synch.o (lock/unlock) + log_ipc.o
logger: src/logger_main.o src/logger.o src/synch.o src/log_ipc.o
	$(CC) $(CFLAGS) -o $@ $^

src/%.o: src/%.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f src/*.o $(ALL) raport.txt *.log

.PHONY: all clean
