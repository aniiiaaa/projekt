#pragma once
#include <sys/types.h>

#define LOG_TEXT_MAX 200

typedef struct log_msg {
    long mtype;                 // 1 = normal, 2 = STOP
    char text[LOG_TEXT_MAX];
} log_msg;

int  log_send(int logid, const char* fmt, ...);
int  log_send_stop(int logid);
