#define _POSIX_C_SOURCE 200809L
#include "log_ipc.h"

#include <sys/msg.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>

static int msgsnd_intr(int qid, const void* msgp, size_t msgsz){
    for(;;){
        if(msgsnd(qid, msgp, msgsz, 0) == 0) return 0;
        if(errno == EINTR) continue;
        return -1;
    }
}

int log_send(int logid, const char* fmt, ...){
    if(logid < 0) return -1;

    log_msg m;
    m.mtype = 1;

    va_list ap;
    va_start(ap, fmt);
    vsnprintf(m.text, sizeof(m.text), fmt, ap);
    va_end(ap);

    m.text[sizeof(m.text)-1] = '\0';
    return msgsnd_intr(logid, &m, sizeof(m.text));
}

int log_send_stop(int logid){
    if(logid < 0) return -1;
    log_msg m;
    m.mtype = 2;
    m.text[0] = '\0';
    return msgsnd_intr(logid, &m, sizeof(m.text));
}
