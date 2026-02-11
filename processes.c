#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <stdarg.h>
#include <sys/msg.h>
#include <sys/ipc.h>
#include <sys/sem.h>

#include "processes.h"
#include "synch.h"
#include "struct.h"
#include "log_ipc.h"

static void L(int logid, const char* fmt, ...){
    if(logid < 0) return;
    char buf[LOG_TEXT_MAX];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);
    buf[sizeof(buf)-1] = '\0';
    (void)log_send(logid, "%s", buf);
}

static void msg_send_resp(int msgid, int nr, int ok, int kurs){
    msg_resp m;
    m.mtype = (long)nr;
    m.resp.ok = ok;
    m.resp.kurs = kurs;

    for(;;){
        if(msgsnd(msgid, &m, sizeof(m.resp), 0) == 0) return;
        if(errno == EINTR) continue;
        if(errno == EIDRM || errno == EINVAL) return;
        perror("msgsnd resp");
        exit(1);
    }
}

static int msg_wait_resp(int msgid, int nr, odpowiedz_kasy *out){
    msg_resp m;
    for(;;){
        ssize_t s = msgrcv(msgid, &m, sizeof(m.resp), (long)nr, 0);
        if(s >= 0){
            *out = m.resp;
            return 1;
        }
        if(errno == EINTR) continue;
        if(errno == EIDRM || errno == EINVAL) return 0;
        perror("msgrcv resp");
        exit(1);
    }
}

/*
 * WERSJA B: GRACEFUL FINISH
 * - NIE usuwamy kolejek msg/log w środku (parent zrobi cleanup_ipc po wyjściu dzieci)
 * - ustawiamy koniec=1 i odtykamy wszystkich semaforami + poison do ring
 * - wysyłamy STOP do loggera
 */
static void finish_all(int id_s, dane *d, int logid){
    int kurs;
    lock(id_s);
    d->koniec = 1;
    kurs = d->kurs;
    unlock(id_s);

    // odetkaj kasę (ring)
    ring_send_poison(id_s, d);

    // odetkaj dysp / kierowcę / pasażerów na wejściach / stanowisko
    for(int i=0;i<16;i++){
        evt_signal(id_s);
        signal_driver(id_s);
        signal_wejscie(id_s, WEJSCIE_A);
        signal_wejscie(id_s, WEJSCIE_B);
        signal_stanowisko(id_s);
    }

    // STOP dla loggera
    (void)log_send_stop(logid);
    L(logid, "[FINISH] koniec=1 kurs=%d", kurs);
}

// ===================== DYSPOZYTOR =====================
void dyspozytor(int id_s, dane *d, int msgid, int logid){
    (void)msgid;
    L(logid, "[DYSP] start pid=%d", (int)getpid());

    for(;;){
        evt_wait(id_s);

        lock(id_s);
        int koniec = d->koniec;
        int pozostalo = d->pozostalo;
        int bus = d->bus_present;
        int kasa_k = d->kasa_koniec;
        int do_wej = d->do_wejscia;
        int budz = 0;

        // WERSJA B: koniec gdy pozostalo==0 (wszyscy przejechali) lub już ustawiono koniec
        if(koniec || pozostalo <= 0){
            unlock(id_s);
            finish_all(id_s, d, logid);
            return;
        }

        // standard: budzenie kierowcy gdy sprzedaż zamknięta i nikt nie wchodzi
        if(bus && kasa_k && do_wej==0 && d->kier_budz==0){
            d->kier_budz = 1;
            budz = 1;
        }
        unlock(id_s);

        if(budz){
            signal_driver(id_s);
            L(logid, "[DYSP] budze kierowce");
        }
    }
}

// ===================== KIEROWCA =====================
void kierowca(int bus_id, int id_s, dane *d, int msgid, int logid){
    (void)msgid;
    L(logid, "[KIER %d] start pid=%d", bus_id, (int)getpid());

    for(;;){
        lock(id_s);
        int koniec = d->koniec;
        int pozostalo = d->pozostalo;
        unlock(id_s);
        if(koniec || pozostalo <= 0) break;

        wait_stanowisko(id_s);

        lock(id_s);
        if(d->koniec || d->pozostalo <= 0){
            unlock(id_s);
            signal_stanowisko(id_s);
            break;
        }

        d->bus_present = 1;
        d->kurs++;
        int kurs_local = d->kurs;

        d->kier_budz = 0;
        d->kasa_koniec = 0;
        d->miejsca = P;
        d->bilety = 0;
        d->do_wejscia = 0;
        d->kasa_budz = 0;
        d->rowery = 0;
        d->czeka_1 = 0;

        unlock(id_s);

        reset_bus_resources(id_s, P, R);
        reset_driver_sem(id_s);

        union semun { int val; struct semid_ds *buf; unsigned short *array; } u;
        u.val = 1;
        (void)semctl(id_s, SEM_SALE, SETVAL, u);

        evt_signal(id_s);
        L(logid, "[KIER %d] start kurs=%d (sale=1)", bus_id, kurs_local);

        // czekaj aż dysp (po info od kasy/pasażerów) obudzi na koniec sprzedaży
        wait_driver(id_s);

        u.val = 0;
        (void)semctl(id_s, SEM_SALE, SETVAL, u);

        // domknij drzwi (mutexy)
        wait_wejscie(id_s, WEJSCIE_A);
        wait_wejscie(id_s, WEJSCIE_B);
        signal_wejscie(id_s, WEJSCIE_A);
        signal_wejscie(id_s, WEJSCIE_B);

        lock(id_s);
        d->bus_present = 0;
        unlock(id_s);

        evt_signal(id_s);
        L(logid, "[KIER %d] ODJAZD kurs=%d (sale=0)", bus_id, kurs_local);

        signal_stanowisko(id_s);
    }

    L(logid, "[KIER %d] exit", bus_id);
    _exit(0);
}

// ===================== KASA =====================
void kasa(int id_s, dane *d, int msgid, int logid){
    L(logid, "[KASA] start pid=%d", (int)getpid());

    for(;;){
        zapytanie_kasy req;

        int okrecv = ring_recv(id_s, d, &req);
        if(!okrecv){
            L(logid, "[KASA] poison -> exit");
            _exit(0);
        }

        lock(id_s);
        int koniec      = d->koniec;
        int zamknieta   = d->blokada;
        int kasa_koniec = d->kasa_koniec;
        int kurs_local  = d->kurs;
        int bus         = d->bus_present;
        int miejsca     = d->miejsca;
        int pozostalo   = d->pozostalo;
        unlock(id_s);

        if(koniec || pozostalo <= 0){
            L(logid, "[KASA] koniec/pozostalo<=0 -> exit");
            _exit(0);
        }

        if(req.nr < 1 || req.nr > ILO_PAS) continue;

        if(zamknieta || kasa_koniec || bus == 0){
            msg_send_resp(msgid, req.nr, 0, kurs_local);
            evt_signal(id_s);
            continue;
        }

        // decyzja o zamknięciu sprzedaży
        int zamykamy = 0;

        if(miejsca <= 0) zamykamy = 1;

        if(!zamykamy && miejsca == 1){
            lock(id_s);
            int c1 = d->czeka_1;
            unlock(id_s);
            if(c1 <= 0) zamykamy = 1;
        }

        if(pozostalo <= 0) zamykamy = 1;

        if(zamykamy){
            int budz = 0;
            int c1 = 0;

            lock(id_s);
            c1 = d->czeka_1;
            d->kasa_koniec = 1;

            if(d->bus_present && d->do_wejscia == 0 && d->kier_budz == 0){
                d->kier_budz = 1;
                budz = 1;
            }
            unlock(id_s);

            evt_signal(id_s);
            if(budz) signal_driver(id_s);

            msg_send_resp(msgid, req.nr, 0, kurs_local);

            L(logid, "[KASA] KONIEC SPRZEDAZY (miejsca=%d pozostalo=%d czeka_1=%d) budz=%d",
              miejsca, pozostalo, c1, budz);
            continue;
        }

        // OK — “zgoda” na próbę wejścia
        msg_send_resp(msgid, req.nr, 1, kurs_local);
        evt_signal(id_s);
    }
}

// ===================== PASAZER =====================
void pasazer(int nr, int id_s, dane *d, int msgid, int logid){
    srand((unsigned)getpid());

    int rower = (rand()%2==1);
    int vip   = (rand()%100 < VIP_PROC);

    int wiek_los = rand()%15;
    int dziecko  = (wiek_los < 8);
    int wiek     = dziecko ? (8 + rand()%7) : wiek_los;

    int potrzebne_m = dziecko ? 2 : 1;
    int potrzebne_r = rower ? 1 : 0;

    L(logid, "[PAS %d] start pid=%d vip=%d rower=%d dziecko=%d",
      nr, (int)getpid(), vip, rower, dziecko);

    int zarejestrowany_kurs = -1;

    for(;;){
        lock(id_s);
        int koniec = d->koniec;
        int zamknieta = d->blokada;
        unlock(id_s);

        if(koniec){
            L(logid, "[PAS %d] koniec=1 -> exit", nr);
            _exit(0);
        }
        if(zamknieta){
            L(logid, "[PAS %d] dworzec zamkniety -> exit", nr);
            _exit(0);
        }

        // czekaj aż sprzedaż otwarta (SEM_SALE==1) — bez busy-wait
        if(sem_wait_one_noconsume(id_s, SEM_SALE) == -1){
            _exit(0);
        }

        // pobierz stan kursu + rejestracja do czeka_1
        lock(id_s);
        int bus = d->bus_present;
        int kasa_koniec = d->kasa_koniec;
        int kurs_local = d->kurs;

        if(zarejestrowany_kurs != kurs_local){
            if(potrzebne_m == 1) d->czeka_1 += 1;
            zarejestrowany_kurs = kurs_local;
        }
        unlock(id_s);

        if(bus == 0 || kasa_koniec == 1){
           
            if(potrzebne_m == 1){
                lock(id_s);
                if(d->czeka_1 > 0) d->czeka_1 -= 1;
                unlock(id_s);
            }
            zarejestrowany_kurs = -1;

           
            continue;
        }

        // request do kasy
        if(!vip){
            zapytanie_kasy req;
            req.pid = getpid();
            req.nr = nr;
            req.wiek = wiek;
            req.czy_dziecko = dziecko;
            req.czy_rower = rower;
            req.czy_vip = 0;

            ring_send(id_s, d, &req);
            evt_signal(id_s);

            odpowiedz_kasy ans;
            if(!msg_wait_resp(msgid, nr, &ans)){
                _exit(0);
            }

            if(ans.ok == 0){
                

                if(potrzebne_m == 1){
                    lock(id_s);
                    if(d->czeka_1 > 0) d->czeka_1 -= 1;
                    unlock(id_s);
                }
                zarejestrowany_kurs = -1;

              
                continue;
            }
        }

        // rezerwacja
        int zarezerwowane = 0;

        lock(id_s);
        int invalid = (d->koniec || d->blokada || d->bus_present==0 || d->kasa_koniec==1);
        int brak_miejsc = (d->miejsca < potrzebne_m);
        int brak_roweru = (d->rowery + potrzebne_r > R);

        if(!invalid && !brak_miejsc && !brak_roweru){
            d->bilety += 1;
            d->miejsca -= potrzebne_m;

            if(potrzebne_r){
                d->rowery += 1;
            }

            d->do_wejscia += potrzebne_m;
            zarezerwowane = 1;
            kurs_local = d->kurs;
        }
        unlock(id_s);

        if(!zarezerwowane){
           
            continue;
        }

        // wejście
        int wej = rower ? WEJSCIE_B : WEJSCIE_A;
        wait_wejscie(id_s, wej);

        int budz = 0;

        lock(id_s);
        d->do_wejscia -= potrzebne_m;
        d->pozostalo  -= 1;

        d->bilety_total += 1;
        if(rower)   d->rowery_total += 1;
        if(dziecko) d->dzieci_total += 1;
        if(vip)     d->vip_total    += 1;

        if(potrzebne_m == 1 && d->czeka_1 > 0) d->czeka_1 -= 1;

        budz = (d->bus_present && d->kasa_koniec==1 && d->do_wejscia==0 && d->kier_budz==0);
        if(budz) d->kier_budz = 1;
        unlock(id_s);

        signal_wejscie(id_s, wej);
        if(budz) signal_driver(id_s);
        evt_signal(id_s);

        L(logid, "[PAS %d] wszedl (kurs=%d vip=%d rower=%d dziecko=%d) -> exit",
          nr, kurs_local, vip, rower, dziecko);

        _exit(0);
    }
}
