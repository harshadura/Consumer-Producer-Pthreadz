/* 
 * File:   main.c
 * Author: madura
 *
 * Created on April 15, 2012, 2:55 PM
 */
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/sem.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
/* needed for wrapper funtions below */
#if !defined(__GNU_LIBRARY__) || defined(_SEM_SEMUN_UNDEFINED)
union semun
{
    int val;
     // value for SETVAL
    struct semid_ds* buf;
     // buffer for IPC_STAT, IPC_SET
    unsigned short* array; // array for GETALL, SETALL
    struct seminfo* __buf; // buffer for IPC_INFO
};
#endif
    
#define key 9090
#define N 5
#define sem_full 0
#define sem_empty 1

/* struct for shared data */
typedef struct _share{
    int buffer[N];
    int current;
}share;

/* upon signals clean up*/
void handle(int i){
    int id = semget(key, 2,  SHM_R | SHM_W);
    if (id != -1 && semctl(id, 0, IPC_RMID, NULL) == -1){
        perror("Error releasing semaphore!");
    }
    id =  shmget(key, sizeof(share), 0777);
    if (id != -1 && shmctl(id, IPC_RMID, NULL)){
        perror("Error releasing shared memory!");
    }
    exit(0);
}
/* wrapper functions for cross process semaphores based on
 * http://neologix.free.fr/unix/shared_memory_semaphores.pdf
 */
void wait(int id, int i) {
    struct sembuf sb;
    sb.sem_num = i;
    sb.sem_op = -1;
    sb.sem_flg = SEM_UNDO;
    semop(id, &sb, 1);
}

void post(int id, int i) {
    struct sembuf sb;
    sb.sem_num = i;
    sb.sem_op = 1;
    sb.sem_flg = SEM_UNDO;
    semop(id, &sb, 1);
}

int init_sem(short int *defaults) {
    union semun arg;
    int id = semget(key, 2,  IPC_CREAT | IPC_EXCL | SHM_R | SHM_W);
    if (id == -1) {
        perror("Semaphores");
        exit(1);
    }
    arg.array = defaults;
    semctl(id, 0, SETALL, arg);
    
    return id;
}


int main(int argc, char** argv) {
    
    signal(SIGKILL, handle);
    signal(SIGABRT, handle);
    signal(SIGINT, handle);
    signal(SIGTERM, handle);
    
    int shmid = shmget(key, sizeof(share), IPC_CREAT | IPC_EXCL | 0777);
    if (shmid == -1) {
        perror("Shared memory ");
        exit(1);
    }
    
    share *s = (share *) shmat(shmid, NULL, 0);
    
    if (s == (void *) -1 ) {
        perror("Shared memory ");
        exit(1);
    }
    
    short int sem_defaults[2];
    sem_defaults[sem_full]=0;
    sem_defaults[sem_empty]=N;
    
    int semid = init_sem(sem_defaults);
    
    while(1) {
        int a;
        printf("Enter a number : ");
        scanf("%d", &a);
        wait(semid, sem_empty);
            s->buffer[s->current] = a;
            s->current++;
        post(semid, sem_full);
    }
    return 0;
}

