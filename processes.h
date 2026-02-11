#pragma once
#include "struct.h"

// TEST wg prowadzącego:
#define ILO_PAS 200

#define P  2
#define R  1
#define N  1
#define VIP_PROC 0

void dyspozytor(int id_s, dane *d, int msgid, int logid);
void kierowca(int bus_id, int id_s, dane *d, int msgid, int logid);
void pasazer(int nr, int id_s, dane *d, int msgid, int logid);
void kasa(int id_s, dane *d, int msgid, int logid);
