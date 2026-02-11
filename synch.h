#pragma once
#include <sys/types.h>
#include <sys/ipc.h>
#include "struct.h"

// semafory stałe
#define BLOKADA        0
#define KIEROWCA       1
#define WEJSCIE_A      2
#define WEJSCIE_B      3
#define STANOWISKO     4

// zasoby autobusu
#define SEM_MIEJSCA    5
#define SEM_ROWERY     6

// ring
#define SEM_RING_MUTEX 7
#define SEM_RING_EMPTY 8
#define SEM_RING_FULL  9

// event dla dyspozytora
#define SEM_EVT        10

// “sprzedaż/boarding otwarte”
#define SEM_SALE       11

#define LICZNIK        12

int create_or_get(key_t key);

void lock(int id_s);
void unlock(int id_s);

void wait_driver(int id_s);
void signal_driver(int id_s);
void reset_driver_sem(int id_s);

void wait_wejscie(int id_s, int ktory);
void signal_wejscie(int id_s, int ktory);

void wait_stanowisko(int id_s);
void signal_stanowisko(int id_s);

void reset_bus_resources(int id_s, int miejsca, int rowery);

// ring
void ring_send(int id_s, dane *d, const zapytanie_kasy *r);
int  ring_recv(int id_s, dane *d, zapytanie_kasy *out);
void ring_send_poison(int id_s, dane *d);

// eventy
void evt_signal(int id_s);
void evt_wait(int id_s);

// helpery
int sem_wait_zero(int id_s, int sem_idx);
int sem_wait_one_noconsume(int id_s, int sem_idx);
