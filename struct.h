#pragma once
#include <sys/types.h>

#define RING_SIZE 32

typedef struct zapytanie_kasy {
    pid_t pid;
    int nr;          // 1..ILO_PAS, a nr<0 = poison/koniec
    int wiek;
    int czy_dziecko;
    int czy_rower;
    int czy_vip;
} zapytanie_kasy;

typedef struct odpowiedz_kasy {
    int ok;
    int kurs;
} odpowiedz_kasy;

typedef struct msg_resp {
    long mtype;      // mtype = nr pasażera
    odpowiedz_kasy resp;
} msg_resp;

typedef struct dane {
    int miejsca;
    int bilety;          // bilety w kursie
    int pozostalo;       // ilu jeszcze nie przejechało
    int rowery;          // rowery w kursie
    int do_wejscia;
    int kasa_koniec;
    int kasa_budz;
    int kurs;
    int kier_budz;
    int koniec;
    int blokada;
    int bus_present;

    int czeka_1;         // ilu w tym kursie potrzebuje 1 miejsca (do decyzji "czy zamykać przy 1 miejscu")

    // ---- sumy globalne (do raportu) ----
    int rowery_total;    // ilu WSIADŁO z rowerem
    int bilety_total;    // ilu WSIADŁO (czyli przejechało)
    int dzieci_total;    // ilu WSIADŁO jako dziecko
    int vip_total;       // ilu WSIADŁO jako VIP

    int ring_head;
    int ring_tail;
    zapytanie_kasy ring[RING_SIZE];
} dane;
