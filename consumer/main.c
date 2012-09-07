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
#include <pthread.h>
#include <semaphore.h>
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

#define ARRMAX 200

typedef struct _share{
    int buffer[N];
    int current;
}share;

int arr[ARRMAX]={0};
int arr_sorted[ARRMAX]={0};
int arr_c=0;
sem_t th_empty;
sem_t th_full;
int semid;
share *sh=NULL;
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

void *thread1(void *arg){ 
    /* read from shared memory */
    while (1) {
        sem_wait(&th_empty);
            int a;

            wait(semid, sem_full);
                sh->current--;
                a = sh->buffer[sh->current];
            post(semid, sem_empty);

            arr[arr_c]=a;
            arr_c++;
            if (arr_c == ARRMAX) {
                printf("Array full, reseting.\n");
                int i;
                for (i=0;i<ARRMAX;i++) {
                    arr[i]=0;
                    arr_sorted[i]=0;
                }
                arr_c = 0;
            } 
        sem_post(&th_full);
    }

}

void *thread2(void *arg){
    /* sort */
    while (1) {
        int i,j;
        /* uses a simplified version of producer-consumer */
        sem_wait(&th_full); 
            /* copy array */
            for (i=0;i<arr_c;i++) {
                arr_sorted[i]=arr[i];
            }
            /* sort */
            for (i=0;i<arr_c;i++) {
                for (j=i+1;j<arr_c;j++){
                    if (arr_sorted[i]>arr_sorted[j]){
                        int tmp = arr_sorted[i];
                        arr_sorted[i] = arr_sorted[j];
                        arr_sorted[j] = tmp;
                    }
                }
            }

            for (i=0;i<arr_c;i++) {
                printf("%d ", arr_sorted[i]);
            }
            printf("\n");
        sem_post(&th_empty);
    }
}
int main(int argc, char** argv) {
    /*threads*/
    pthread_t th1,th2;
    
    /* find shared memory, if not found fail */
    int shmid = shmget(key, sizeof(share), 0777);
    if (shmid == -1) {
        perror("Shared memory ");
        exit(1);
    }
    
    /* attach shared mem to process address space(map pages to the process) */
    sh = (share *) shmat(shmid, NULL, 0);
    if (sh == (void *) -1 ) {
        perror("Shared memory ");
        exit(1);
    }
    
    /* find cross process semaphores, if not fail */
    semid = semget(key, 2, SHM_R | SHM_W);
    if (semid == -1) {
        perror("Semaphores ");
        exit(1);
    }
    
    /* initialize semaphores that are used by threads */
    sem_init(&th_empty, 0, 1);
    sem_init(&th_full, 0, 0);

    /* create threads */
    pthread_create(&th1,NULL,thread1,NULL);
    pthread_create(&th2,NULL,thread2,NULL);
    pthread_join(th1, NULL);
    pthread_join(th2, NULL);
    
    return 0;
}

