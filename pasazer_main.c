#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <sys/shm.h>

#include "struct.h"
#include "processes.h"

int main(int argc, char **argv){
    if(argc != 6){
        fprintf(stderr, "Uzycie: %s nr_pasazera id_sem id_shm id_msg id_log\n", argv[0]);
        return 1;
    }

    int nr     = atoi(argv[1]);
    int id_s   = atoi(argv[2]);
    int id_shm = atoi(argv[3]);
    int msgid  = atoi(argv[4]);
    int logid  = atoi(argv[5]);

    dane *d = shmat(id_shm, NULL, 0);
    if(d==(void*)-1){
        perror("shmat");
        return 1;
    }

    pasazer(nr, id_s, d, msgid, logid);
    return 0;
}
