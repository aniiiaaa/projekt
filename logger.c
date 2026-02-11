#define _POSIX_C_SOURCE 200809L
#include "logger.h"
#include "log_ipc.h"
#include "struct.h"
#include "synch.h"

#include <sys/msg.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <errno.h>
#include <time.h>

static void ts(char *buf, size_t n){
    time_t t = time(NULL);
    struct tm tm;
    localtime_r(&t, &tm);
    strftime(buf, n, "%Y-%m-%d %H:%M:%S", &tm);
}

void logger_run(int id_s, dane *d, int logid){
    FILE *f = fopen("raport.txt", "a");
    if(!f){ perror("fopen raport.txt"); _exit(1); }
    setvbuf(f, NULL, _IONBF, 0);

    fprintf(f, "=== LOGGER START (logid=%d) ===\n", logid);

    for(;;){
        log_msg m;
        ssize_t s = msgrcv(logid, &m, sizeof(m.text), 0, 0);
        if(s >= 0){
            char stamp[64];
            ts(stamp, sizeof(stamp));

            if(m.mtype == 2){
                // STOP -> dopisz podsumowanie
                int kurs, pozostalo, przejechalo, dzieci, vip, rowery;

                lock(id_s);
                kurs       = d->kurs;
                pozostalo  = d->pozostalo;
                przejechalo= d->bilety_total;
                dzieci     = d->dzieci_total;
                vip        = d->vip_total;
                rowery     = d->rowery_total;
                unlock(id_s);

                fprintf(f, "[%s] STOP\n", stamp);
                fprintf(f, "\n=== PODSUMOWANIE ===\n");
                fprintf(f, "Kursy (ostatni nr): %d\n", kurs);
                fprintf(f, "Przejechalo (bilety_total): %d\n", przejechalo);
                fprintf(f, "Dzieci: %d\n", dzieci);
                fprintf(f, "VIP: %d\n", vip);
                fprintf(f, "Rowery: %d\n", rowery);
                fprintf(f, "Pozostalo do obsluzenia: %d\n", pozostalo);
                fprintf(f, "====================\n\n");

                break;
            }

            fprintf(f, "[%s] %s\n", stamp, m.text);
            continue;
        }

        if(errno == EINTR) continue;
        if(errno == EIDRM || errno == EINVAL) break;

        perror("msgrcv logger");
        break;
    }

    fprintf(f, "=== LOGGER END ===\n\n");
    fclose(f);
    _exit(0);
}
