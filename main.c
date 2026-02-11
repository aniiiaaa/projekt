#define _POSIX_C_SOURCE 200809L
#include "processes.h"
#include "synch.h"
#include "struct.h"
#include "log_ipc.h"

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/wait.h>
#include <sys/sem.h>
#include <sys/msg.h>
#include <errno.h>
#include <string.h>
#include <sys/time.h>

static volatile sig_atomic_t przerwij = 0;
static volatile sig_atomic_t tick = 0;

static dane *wspolne = NULL;
static int id_semafory = -1;
static int id_pamiec   = -1;
static int id_msg      = -1;
static int id_log      = -1;

static pid_t *dzieci = NULL;
static int liczba_dzieci = 0;
static int pojemnosc_dzieci = 0;

/* ====== NOWE: osobna lista PIDów pasażerów ====== */
static pid_t *pas_pids = NULL;
static int pas_alive = 0;
static int pas_cap = 0;

static void add_pas(pid_t pid){
    if(pid <= 0) return;
    if(pas_alive >= pas_cap){
        int nowa = (pas_cap == 0) ? 256 : (pas_cap * 2);
        pid_t *tmp = realloc(pas_pids, (size_t)nowa * sizeof(*pas_pids));
        if(!tmp){ perror("realloc pas_pids"); exit(1); }
        pas_pids = tmp;
        pas_cap = nowa;
    }
    pas_pids[pas_alive++] = pid;
}

static int is_pas_pid(pid_t pid){
    for(int i=0; i<pas_alive; i++){
        if(pas_pids[i] == pid) return 1;
    }
    return 0;
}

static void remove_pas(pid_t pid){
    for(int i=0; i<pas_alive; i++){
        if(pas_pids[i] == pid){
            pas_pids[i] = pas_pids[pas_alive-1];
            pas_alive--;
            return;
        }
    }
}
/* =============================================== */

static void dodaj_dziecko(pid_t pid){
    if(pid <= 0) return;
    if(liczba_dzieci >= pojemnosc_dzieci){
        int nowa = (pojemnosc_dzieci == 0) ? 64 : (pojemnosc_dzieci * 2);
        pid_t *tmp = realloc(dzieci, (size_t)nowa * sizeof(*dzieci));
        if(!tmp){ perror("realloc"); exit(1); }
        dzieci = tmp;
        pojemnosc_dzieci = nowa;
    }
    dzieci[liczba_dzieci++] = pid;
}

static void on_stop(int sig){ (void)sig; przerwij = 1; }
static void on_tick(int sig){ (void)sig; tick = 1; }

static void set_handler(int sig, void (*fn)(int)){
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = fn;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    if(sigaction(sig, &sa, NULL) == -1){
        perror("sigaction");
        exit(1);
    }
}

static void start_timer_1s(void){
    struct itimerval it;
    memset(&it, 0, sizeof(it));
    it.it_interval.tv_sec = 1;
    it.it_value.tv_sec = 1;
    if(setitimer(ITIMER_REAL, &it, NULL) == -1){
        perror("setitimer");
        exit(1);
    }
}

static int odtworz_shm(key_t k, size_t sz){
    int id = shmget(k, sz, IPC_CREAT | IPC_EXCL | 0600);
    if(id != -1) return id;
    if(errno == EEXIST){
        id = shmget(k, 0, 0600);
        if(id == -1){ perror("shmget existing"); exit(1); }
        if(shmctl(id, IPC_RMID, NULL) == -1){ perror("shmctl RMID"); exit(1); }
        id = shmget(k, sz, IPC_CREAT | IPC_EXCL | 0600);
        if(id == -1){ perror("shmget recreate"); exit(1); }
        return id;
    }
    perror("shmget");
    exit(1);
}

static int odtworz_msg(key_t k){
    int id = msgget(k, IPC_CREAT | IPC_EXCL | 0600);
    if(id != -1) return id;
    if(errno == EEXIST){
        id = msgget(k, 0600);
        if(id == -1){ perror("msgget existing"); exit(1); }
        if(msgctl(id, IPC_RMID, NULL) == -1){ perror("msgctl RMID"); exit(1); }
        id = msgget(k, IPC_CREAT | IPC_EXCL | 0600);
        if(id == -1){ perror("msgget recreate"); exit(1); }
        return id;
    }
    perror("msgget");
    exit(1);
}

static int odtworz_semafory(key_t key){
    int sid = semget(key, LICZNIK, IPC_CREAT | IPC_EXCL | 0600);
    if(sid != -1){
        semctl(sid, 0, IPC_RMID);
        return create_or_get(key);
    }
    if(errno == EEXIST){
        sid = semget(key, LICZNIK, 0600);
        if(sid == -1){ perror("semget existing"); exit(1); }
        if(semctl(sid, 0, IPC_RMID) == -1){ perror("semctl RMID"); exit(1); }
        return create_or_get(key);
    }
    perror("semget");
    exit(1);
}

static int odtworz_log(key_t k){
    int id = msgget(k, IPC_CREAT | IPC_EXCL | 0600);
    if(id != -1) return id;
    if(errno == EEXIST){
        id = msgget(k, 0600);
        if(id == -1){ perror("msgget log existing"); exit(1); }
        if(msgctl(id, IPC_RMID, NULL) == -1){ perror("msgctl log RMID"); exit(1); }
        id = msgget(k, IPC_CREAT | IPC_EXCL | 0600);
        if(id == -1){ perror("msgget log recreate"); exit(1); }
        return id;
    }
    perror("msgget log");
    exit(1);
}

static void cleanup_ipc(void){
    if(wspolne && wspolne != (void*)-1){
        shmdt(wspolne);
        wspolne = NULL;
    }
    if(id_pamiec != -1){
        shmctl(id_pamiec, IPC_RMID, NULL);
        id_pamiec = -1;
    }
    if(id_semafory != -1){
        semctl(id_semafory, 0, IPC_RMID);
        id_semafory = -1;
    }
    if(id_msg != -1){
        msgctl(id_msg, IPC_RMID, NULL);
        id_msg = -1;
    }
    if(id_log != -1){
        msgctl(id_log, IPC_RMID, NULL);
        id_log = -1;
    }
}

static void child_setup(void){
    signal(SIGINT, SIG_IGN);
    signal(SIGTERM, SIG_DFL);
}

static int children_alive(void){
    int alive = 0;
    for(int i=0; i<liczba_dzieci; i++){
        if(dzieci[i] > 0) alive++;
    }
    return alive;
}

static void mark_dead(pid_t pid){
    for(int i=0; i<liczba_dzieci; i++){
        if(dzieci[i] == pid){ dzieci[i] = -1; return; }
    }
}

/*
 * graceful shutdown
 * - ustaw koniec=1
 * - poison do ring (kasa wyjdzie z ring_recv)
 * - broadcast semaforami (dysp/kier/pasażer)
 * - STOP do loggera
 */
static void request_finish_all(void){
    if(id_semafory != -1 && wspolne){
        lock(id_semafory);
        wspolne->koniec = 1;
        unlock(id_semafory);

        ring_send_poison(id_semafory, wspolne);

        for(int i=0; i<16; i++){
            evt_signal(id_semafory);
            signal_driver(id_semafory);
            signal_wejscie(id_semafory, WEJSCIE_A);
            signal_wejscie(id_semafory, WEJSCIE_B);
            signal_stanowisko(id_semafory);
        }
    }

    if(id_log != -1){
        (void)log_send_stop(id_log);
    }
}

int main(void){
    setvbuf(stdout, NULL, _IONBF, 0);

    set_handler(SIGINT,  on_stop);
    set_handler(SIGTERM, on_stop);
    set_handler(SIGALRM, on_tick);
    start_timer_1s();

    key_t k_shm = ftok("main.c", 'A');
    key_t k_sem = ftok("main.c", 'B');
    key_t k_msg = ftok("main.c", 'C');
    key_t k_log = ftok("main.c", 'L');
    if(k_shm == -1 || k_sem == -1 || k_msg == -1 || k_log == -1){
        perror("ftok");
        exit(1);
    }

    id_pamiec = odtworz_shm(k_shm, sizeof(dane));
    wspolne = (dane*)shmat(id_pamiec, NULL, 0);
    if(wspolne == (void*)-1){ perror("shmat"); cleanup_ipc(); exit(1); }

    id_semafory = odtworz_semafory(k_sem);
    id_msg = odtworz_msg(k_msg);
    id_log = odtworz_log(k_log);

    // init danych
    lock(id_semafory);
    wspolne->miejsca = P;
    wspolne->bilety = 0;
    wspolne->pozostalo = ILO_PAS;
    wspolne->rowery = 0;
    wspolne->do_wejscia = 0;
    wspolne->kasa_koniec = 0;
    wspolne->kurs = 0;
    wspolne->kier_budz = 0;
    wspolne->koniec = 0;
    wspolne->blokada = 0;
    wspolne->bus_present = 0;

    wspolne->ring_head = 0;
    wspolne->ring_tail = 0;

    wspolne->czeka_1 = 0;

    wspolne->rowery_total = 0;
    wspolne->bilety_total = 0;
    wspolne->dzieci_total = 0;
    wspolne->vip_total = 0;
    unlock(id_semafory);

    char sem_txt[32], shm_txt[32], msg_txt[32], log_txt[32];
    snprintf(sem_txt, sizeof(sem_txt), "%d", id_semafory);
    snprintf(shm_txt, sizeof(shm_txt), "%d", id_pamiec);
    snprintf(msg_txt, sizeof(msg_txt), "%d", id_msg);
    snprintf(log_txt, sizeof(log_txt), "%d", id_log);

    // logger
    {
        pid_t pid = fork();
        if(pid == 0){
            child_setup();
            execl("./logger", "logger", sem_txt, shm_txt, log_txt, (char*)NULL);
            perror("execl logger");
            _exit(2);
        }
        dodaj_dziecko(pid);
    }

    // kierowca
    for(int bus=1; bus<=N; bus++){
        pid_t pid = fork();
        if(pid == 0){
            child_setup();
            char bus_txt[16];
            snprintf(bus_txt, sizeof(bus_txt), "%d", bus);
            execl("./kierowca", "kierowca", bus_txt, sem_txt, shm_txt, msg_txt, log_txt, (char*)NULL);
            perror("execl kierowca");
            _exit(2);
        }
        dodaj_dziecko(pid);
    }

    // kasa
    {
        pid_t pid = fork();
        if(pid == 0){
            child_setup();
            execl("./kasa", "kasa", sem_txt, shm_txt, msg_txt, log_txt, (char*)NULL);
            perror("execl kasa");
            _exit(2);
        }
        dodaj_dziecko(pid);
    }

    // dyspozytor
    {
        pid_t pid = fork();
        if(pid == 0){
            child_setup();
            execl("./dyspozytor", "dyspozytor", sem_txt, shm_txt, msg_txt, log_txt, (char*)NULL);
            perror("execl dyspozytor");
            _exit(2);
        }
        dodaj_dziecko(pid);
    }

    // pasazerowie
    for(int nr=1; nr<=ILO_PAS; nr++){
        pid_t pid = fork();
        if(pid == 0){
            child_setup();
            char nr_txt[16];
            snprintf(nr_txt, sizeof(nr_txt), "%d", nr);
            execl("./pasazer", "pasazer", nr_txt, sem_txt, shm_txt, msg_txt, log_txt, (char*)NULL);
            perror("execl pasazer");
            _exit(2);
        }
        dodaj_dziecko(pid);
        add_pas(pid); /* <-- KLUCZOWE: trackujemy pasażerów */
    }

    int finish_requested = 0;

    // PARENT loop
    while(1){
        int status;
        pid_t w = waitpid(-1, &status, 0);

        if(w > 0){{
            mark_dead(w);

            if(is_pas_pid(w)){
                remove_pas(w);

                // jeśli wszyscy pasażerowie skończyli -> kończymy całość
                if(pas_alive == 0 && !finish_requested){
                    request_finish_all();
                    finish_requested = 1;
                    } else if(w == -1) {
            if(errno == EINTR){
                // sygnał SIGALRM/SIGINT/SIGTERM: obsługa niżej
            } else if(errno == ECHILD){
                break;
            } else {
                perror("waitpid");
                break;
            }
                }
            }
        }

        if(finish_requested) break;
        if(children_alive() == 0) break;

        if(przerwij){
            if(!finish_requested){
                request_finish_all();
                finish_requested = 1;
            }
            break;
        }

        if(tick){
            tick = 0;

            int koniec;
            lock(id_semafory);
            koniec = wspolne->koniec;
            unlock(id_semafory);

            // NIE bazujemy na pozostalo (bo może się rozjechać)
            if(koniec && !finish_requested){
                request_finish_all();
                finish_requested = 1;
                break;
            }
        }

      
    }

    // po request_finish_all(): poczekaj aż wszystkie dzieci same wyjdą
    while(wait(NULL) > 0) {}

    cleanup_ipc();
    free(dzieci);
    free(pas_pids);
    return 0;
}
