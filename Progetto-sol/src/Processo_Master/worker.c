#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>
#include <signal.h>
#include <util.h>
#include <queue.h>
#include <worker.h>
#include <master_worker.h>
#include <conn.h>
extern pthread_mutex_t lock_conn;
long risultato(char *file,FILE *ifp){
    struct stat info;
    size_t dim_file;
    size_t dim_n=sizeof (long);
    size_t da_leggere;
    long somma=0;
    if(stat(file,&info)==-1){
        perror("stat");
        exit(EXIT_FAILURE);
    }
    dim_file=info.st_size;
    da_leggere=dim_file/dim_n;//mi dice quanti numeri sono contenuti nel file
    long buffer[da_leggere+1];
    //in ogni cella c'è un numero
    if(fread(buffer,dim_n,da_leggere+1,ifp)!=da_leggere){
        perror("read file");
    }
    //calcolo il risultato in base alla formula descritta nel progetto
    for(int i=0; i<da_leggere; i++){
        somma=somma+(i*buffer[i]);
    }
    return somma;
}
void* threadfunc(void* arg){
    int fd_c;
    typedef struct thread_arg Thread_args;
    Thread_args *Thread_Args=(Thread_args*)arg;
    fd_c=Thread_Args->fd_c;
    Queue_t *coda_task=Thread_Args->coda_task;
    FILE *ifp;
    char letto[6];
    long somma;
    //fino a quando il thread non legge la stringa di EOF estrae elementi dalla coda
    while (1){
        char *file=pop(coda_task);
        //la stringa finito rappresenta la stringa di EOF
        if(strcmp(file,"finito")==0){
            break;
        }
        else{
            if((ifp=fopen(file,"rb"))==NULL){
                perror("Aprendo file");
                exit(EXIT_FAILURE);
            }
            else{
            //mutua esclusione
                LOCK(&lock_conn);
                int r;
                somma=risultato(file,ifp);
                fclose(ifp);
                int lung= strlen(file)+1;
                //per ogni write si attende che il collector ci invii un messaggio di conferma
                SYSCALL_EXIT("write 1 master",r,writen(fd_c, &lung, sizeof (int)),"write lunghezza","")
                SYSCALL_EXIT("read 1 master",r,readn(fd_c,letto,5),"Read conferma","")
                SYSCALL_EXIT("write 2 master",r,writen(fd_c, file, lung*sizeof (char)),"write file","")
                SYSCALL_EXIT("read 2 master",r,readn(fd_c,letto,5),"Read conferma 2","")
                SYSCALL_EXIT("write 3 master",r,writen(fd_c, &somma, sizeof (long)),"write risultato","")
                SYSCALL_EXIT("read 3 master",r,readn(fd_c,letto,5),"Read conferma 3","")  
                UNLOCK(&lock_conn);
                free(file);
            }
        }
    }
    return NULL;
}
