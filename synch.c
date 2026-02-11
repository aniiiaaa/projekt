#define _POSIX_C_SOURCE 200809L
#include "synch.h"
#include "processes.h"

#include <stdio.h>
#include <stdlib.h>
#include <sys/sem.h>
#include <errno.h>

union semun {
    int val;
    struct semid_ds *buf;
    unsigned short *array;
};

static void sem_down(int id, int nr){
    struct sembuf op = { .sem_num=(unsigned short)nr, .sem_op=-1, .sem_flg=0 };
    for(;;){
        if(semop(id, &op, 1) == 0) return;
        if(errno == EINTR) continue;
        perror("semop down");
        exit(1);
    }
}

static void sem_up(int id, int nr){
    struct sembuf op = { .sem_num=(unsigned short)nr, .sem_op=+1, .sem_flg=0 };
    for(;;){
        if(semop(id, &op, 1) == 0) return;
        if(errno == EINTR) continue;
        perror("semop up");
        exit(1);
    }
}

int create_or_get(key_t key){
    int sid = semget(key, LICZNIK, IPC_CREAT | IPC_EXCL | 0600);
    if(sid != -1){
        union semun u;

        u.val = 1; if(semctl(sid, BLOKADA, SETVAL, u)==-1){perror("BLOKADA");exit(1);}
        u.val = 0; if(semctl(sid, KIEROWCA, SETVAL, u)==-1){perror("KIEROWCA");exit(1);}
        u.val = 1; if(semctl(sid, WEJSCIE_A, SETVAL, u)==-1){perror("WEJSCIE_A");exit(1);}
        u.val = 1; if(semctl(sid, WEJSCIE_B, SETVAL, u)==-1){perror("WEJSCIE_B");exit(1);}
        u.val = 1; if(semctl(sid, STANOWISKO, SETVAL, u)==-1){perror("STANOWISKO");exit(1);}

        u.val = P; if(semctl(sid, SEM_MIEJSCA, SETVAL, u)==-1){perror("SEM_MIEJSCA");exit(1);}
        u.val = R; if(semctl(sid, SEM_ROWERY, SETVAL, u)==-1){perror("SEM_ROWERY");exit(1);}

        u.val = 1;         if(semctl(sid, SEM_RING_MUTEX, SETVAL, u)==-1){perror("RING_MUTEX");exit(1);}
        u.val = RING_SIZE; if(semctl(sid, SEM_RING_EMPTY, SETVAL, u)==-1){perror("RING_EMPTY");exit(1);}
        u.val = 0;         if(semctl(sid, SEM_RING_FULL,  SETVAL, u)==-1){perror("RING_FULL");exit(1);}

        u.val = 0;         if(semctl(sid, SEM_EVT, SETVAL, u)==-1){perror("SEM_EVT");exit(1);}

        // sprzedaż startuje zamknięta
        u.val = 0;         if(semctl(sid, SEM_SALE, SETVAL, u)==-1){perror("SEM_SALE");exit(1);}

        return sid;
    }

    if(errno == EEXIST){
        sid = semget(key, LICZNIK, 0600);
        if(sid == -1){ perror("semget existing"); exit(1); }
        return sid;
    }

    perror("semget");
    exit(1);
}

void lock(int id_s){ sem_down(id_s, BLOKADA); }
void unlock(int id_s){ sem_up(id_s, BLOKADA); }

void wait_driver(int id_s){ sem_down(id_s, KIEROWCA); }
void signal_driver(int id_s){ sem_up(id_s, KIEROWCA); }

void reset_driver_sem(int id_s){
    union semun u; u.val = 0;
    if(semctl(id_s, KIEROWCA, SETVAL, u)==-1){ perror("reset_driver_sem"); exit(1); }
}

void wait_wejscie(int id_s, int ktory){ sem_down(id_s, ktory); }
void signal_wejscie(int id_s, int ktory){ sem_up(id_s, ktory); }

void wait_stanowisko(int id_s){ sem_down(id_s, STANOWISKO); }
void signal_stanowisko(int id_s){ sem_up(id_s, STANOWISKO); }

void reset_bus_resources(int id_s, int miejsca, int rowery){
    union semun u;
    u.val = miejsca; if(semctl(id_s, SEM_MIEJSCA, SETVAL, u)==-1){ perror("set miejsca"); exit(1); }
    u.val = rowery;  if(semctl(id_s, SEM_ROWERY, SETVAL, u)==-1){ perror("set rowery"); exit(1); }
}

// ===== RING =====
void ring_send(int id_s, dane *d, const zapytanie_kasy *r){
    sem_down(id_s, SEM_RING_EMPTY);
    sem_down(id_s, SEM_RING_MUTEX);

    d->ring[d->ring_tail] = *r;
    d->ring_tail = (d->ring_tail + 1) % RING_SIZE;

    sem_up(id_s, SEM_RING_MUTEX);
    sem_up(id_s, SEM_RING_FULL);
}

int ring_recv(int id_s, dane *d, zapytanie_kasy *out){
    sem_down(id_s, SEM_RING_FULL);
    sem_down(id_s, SEM_RING_MUTEX);

    *out = d->ring[d->ring_head];
    d->ring_head = (d->ring_head + 1) % RING_SIZE;

    sem_up(id_s, SEM_RING_MUTEX);
    sem_up(id_s, SEM_RING_EMPTY);

    if(out->nr < 0) return 0; // poison
    return 1;
}

void ring_send_poison(int id_s, dane *d){
    zapytanie_kasy r;
    r.pid = 0;
    r.nr = -1;
    r.wiek = 0;
    r.czy_dziecko = 0;
    r.czy_rower = 0;
    r.czy_vip = 0;
    ring_send(id_s, d, &r);
}

// ===== EVENT =====
void evt_signal(int id_s){
    sem_up(id_s, SEM_EVT);
}

void evt_wait(int id_s){
    struct sembuf op = { .sem_num=(unsigned short)SEM_EVT, .sem_op=-1, .sem_flg=0 };
    for(;;){
        if(semop(id_s, &op, 1) == 0) return;
        if(errno == EINTR) continue;
        perror("evt_wait semop");
        exit(1);
    }
}

// ===== HELPERS =====
int sem_wait_zero(int id_s, int sem_idx){
    struct sembuf op = { .sem_num=(unsigned short)sem_idx, .sem_op=0, .sem_flg=0 };
    for(;;){
        if(semop(id_s, &op, 1) == 0) return 0;
        if(errno == EINTR) continue;
        if(errno == EIDRM || errno == EINVAL) return -1;
        perror("sem_wait_zero");
        exit(1);
    }
}

int sem_wait_one_noconsume(int id_s, int sem_idx){
    struct sembuf ops[2] = {
        { .sem_num=(unsigned short)sem_idx, .sem_op=-1, .sem_flg=0 },
        { .sem_num=(unsigned short)sem_idx, .sem_op=+1, .sem_flg=0 }
    };
    for(;;){
        if(semop(id_s, ops, 2) == 0) return 0;
        if(errno == EINTR) continue;
        if(errno == EIDRM || errno == EINVAL) return -1;
        perror("sem_wait_one_noconsume");
        exit(1);
    }
}
