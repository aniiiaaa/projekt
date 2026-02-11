CC=gcc
CFLAGS=-Wall -Wextra -O2 -std=c11 -Iinclude

ALL=main pasazer kasa kierowca dyspozytor logger
OBJ_COMMON=synch.o processes.o log_ipc.o

all: $(ALL)

main: main.o $(OBJ_COMMON)
	$(CC) $(CFLAGS) -o $@ $^

pasazer: pasazer_main.o $(OBJ_COMMON)
	$(CC) $(CFLAGS) -o $@ $^

kasa: kasa_main.o $(OBJ_COMMON)
	$(CC) $(CFLAGS) -o $@ $^

kierowca: kierowca_main.o $(OBJ_COMMON)
	$(CC) $(CFLAGS) -o $@ $^

dyspozytor: dyspozytor_main.o $(OBJ_COMMON)
	$(CC) $(CFLAGS) -o $@ $^

# logger potrzebuje synch.o (lock/unlock) + log_ipc.o
logger: logger_main.o logger.o synch.o log_ipc.o
	$(CC) $(CFLAGS) -o $@ $^

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f *.o $(ALL) raport.txt *.log

.PHONY: all clean
