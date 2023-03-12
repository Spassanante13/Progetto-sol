#include <stdlib.h>
#include <stdio.h>
#include <pthread.h>
#include <getopt.h>
#include <dirent.h>
#include <time.h>
#include <errno.h>
#include <master_worker.h>
#include <util.h>
#include <queue.h>
#include <worker.h>
#define MAX_LENG 255
#define N_THREAD 4
#define MAX_LEN 8
struct stat info;
extern pthread_mutex_t lock_term;
extern int termina;
//funzione per creare il ritardo tra l'inserimento in coda per la comunicazione dei thread worer tra un inseritmento e l'altro
int sleepdelay(long delay){
    struct timespec ts;
    int res;
    if (delay < 0){
        errno = EINVAL;
        return -1;
    }
    ts.tv_sec = delay / 1000;
    ts.tv_nsec = (delay % 1000) * 1000000;
    do {
        res = nanosleep(&ts, &ts);//utilizzo della funzione nanosec per essere uscire dall'attesa se arriva un'interruzione
    } while (res && errno == EINTR);
    return res;
}
//funzione che ci permette essere sicuri di non aprire le directory . e ..
int isdot(const char dir[]){
    if((strncmp(".", dir,1) == 0) || (strncmp("..", dir,2) == 0))
        return 1;

    return 0;
}
//funzione che controlla se il file passato come argomento sia effettivamente un file regolare
int check_file_reg(char *nomefile){
    int r;
    SYSCALL_EXIT(stat,r, stat(nomefile,&info),"facendo stat di %s: errno=%d\n",nomefile,errno)
    if(S_ISREG(info.st_mode)){
        return 0;
    } else{
        return -1;
    }

}
/*
funzione che ci permette di controllare se è arrivata un'interruzione al thread sighandler
in modo tale da non inserire più file nella coda per la comunicazione con i thread worker
*/
int check_termina(){
	LOCK(&lock_term);
	int ritorna=0;
	if(termina==-1){
		ritorna=-1;
	}
	UNLOCK(&lock_term);
	return ritorna;
}
//funzione che controlla se l'argomento in -d è effettivamente una directory
int check_dir(char *nomedir){
    int r;
    SYSCALL_EXIT(stat,r, stat(nomedir,&info),"facendo stat di %s: errno=%d\n",nomedir,errno)
    if(S_ISDIR(info.st_mode)){
        return 0;
    }
    else{
        return -1;
    }
}
//funzione ricorsiva per la ricerca di file regolari all'interno della directory passata come argomento
void naviga_dir(char nomedir[],Queue_t* coda_task, long delay){
    DIR *dir;
    int r;
    if((dir = opendir(nomedir)) == NULL)
    {
        perror("opendir");
        print_error("Errore aprendo la directory %s\n", nomedir);
        return;
    }
    else
    {
        struct dirent *file;
        while((errno = 0, file = readdir(dir)) != NULL && check_termina()!=-1)
        {
            char file_name[MAX_LENG+1];
            int len1 = strlen(nomedir);
            int len2 = strlen(file->d_name);
            if((len1 + len2 + 2) > MAX_LENG)
            {
                fprintf(stderr, "ERRORE: MAX_LEN troppo piccolo\n");
                exit(EXIT_FAILURE);
            }
            if(isdot(file->d_name)==0){
                strncpy(file_name, nomedir, MAX_LENG);
                strncat(file_name, "/", MAX_LENG - len1 - 1);
                strncat(file_name, file->d_name, MAX_LENG - len1 - 2);
                SYSCALL_EXIT(stat,r, stat(file_name,&info),"facendo stat di %s: errno=%d\n",file_name,errno)
               
                if(S_ISDIR(info.st_mode))
                {
                    naviga_dir(file_name,coda_task,delay);
                }
                else
               { 	
               	sleepdelay(delay);
               	char *data=malloc((MAX_LENG+1)*sizeof(char));
               	if(data==NULL){
               		perror("errore malloc");
               		exit(EXIT_FAILURE);
               	}
               	strncpy(data,file_name,(MAX_LENG+1));
               	push(coda_task,data);       
               }
            }
        }
        if(errno != 0){
            perror("readdir");
        }
        closedir(dir);
    }
}
void master_worker(int argc, char* argv[],int fd_c){
    long numero_thread=N_THREAD;
    long max_coda=MAX_LEN;
    long delay=0;
    int opt;
    int n_inserito=-1;
    int q_inserito=-1;
    int t_inserito=-1;
    char *nomedir=NULL;
    int is_dir;
    Queue_t *coda_task_concorrente;
    typedef struct thread_arg Thread_arg;
    Thread_arg *Thread_Args;
    pthread_t *threadpool;
    while((opt= getopt(argc,argv,"n:q:d:t:"))!=-1){
        switch (opt) {
            case 'n':
                n_inserito=isNumber(optarg,&numero_thread);
                break;
            case 'q':
                q_inserito= isNumber(optarg,&max_coda);
                break;
            case 't':
                t_inserito= isNumber(optarg,&delay);
                break;
            case 'd':
                nomedir= malloc(256*sizeof (char ));
                if(nomedir==NULL){
                	perror("errore malloc");
                	exit(EXIT_FAILURE);
               }
                strcpy(nomedir,optarg);
                break;
            default:
                printf("Inseriti male\n");
                break;
        }
    }
    if(q_inserito==-1){
    //inizializzazione della coda
    	coda_task_concorrente= initQueue(MAX_LEN);	

    } else{
    //inizializzazione della coda
        coda_task_concorrente= initQueue(max_coda);
    }
    if(t_inserito==-1){
    	delay=0;
    }
     //allocazione e creazione del threadpool si controlla se sono stati  passati argomenti -n in input 
    if(n_inserito!=-1){
    	threadpool=malloc(numero_thread*sizeof(pthread_t));
    	if(threadpool==NULL){
            perror("Malloc errore");
            exit(EXIT_FAILURE);
        }
        //struttura che contiene gli argomenti da passare al thread; gli argomenti sono: la coda per comunicare e la connessione sulla quale inviare i risultati al collector
        Thread_Args= malloc(numero_thread*sizeof (Thread_arg));
        if(Thread_Args==NULL){
        	perror("Malloc errore");
               exit(EXIT_FAILURE);
        }
        for(int j=0; j<numero_thread; j++){
            Thread_Args[j].coda_task=coda_task_concorrente;
            Thread_Args[j].fd_c=fd_c;
        }
        for(int i=0; i<numero_thread;  i++){
            if(pthread_create(&threadpool[i],NULL,&threadfunc,Thread_Args)!=0){
            	perror("errore creazione thread");
            	exit(EXIT_FAILURE);
            }
        }
    }
    else{
        threadpool=malloc(N_THREAD*sizeof(pthread_t));  
        if(threadpool==NULL){
            perror("Malloc errore");
            exit(EXIT_FAILURE);
        }
        //struttura che contiene gli argomenti da passare al thread; gli argomenti sono: la coda per comunicare e la connessione sulla quale inviare i risultati al collector
    	Thread_Args=malloc(N_THREAD*sizeof(Thread_arg));
    	if(Thread_Args==NULL){
        	perror("Malloc errore");
               exit(EXIT_FAILURE);
        }
        for(int j=0; j<numero_thread; j++){
            Thread_Args[j].coda_task=coda_task_concorrente;
            Thread_Args[j].fd_c=fd_c;
        }
        
        for(int i=0; i<N_THREAD;  i++){
          if(pthread_create(&threadpool[i],NULL,&threadfunc,Thread_Args)!=0){
            	perror("errore creazione thread");
            	exit(EXIT_FAILURE);
            }
        }
    }
   if(nomedir!=NULL){
        is_dir= check_dir(nomedir);
        if(is_dir==-1){
            printf("Hai inserito una directory non valida\n");
        }
        else{
            naviga_dir(nomedir,coda_task_concorrente,delay);
        }
        free(nomedir);
    }
    //ciclo per ottenere i file passati in input
    while(argv[optind]!=NULL && strncmp(argv[optind],"-d",2)!=0 && strncmp(argv[optind],"-t",2)!=0 && check_termina()!=-1){
    	char *data=malloc((MAX_LENG+1)*sizeof(char));
    	if(data==NULL){
            perror("errore malloc");
            exit(EXIT_FAILURE);
        }
    	strncpy(data,argv[optind],MAX_LENG+1);
        if(check_file_reg(argv[optind])==-1){
            printf("%s Non è un file regolare\n",argv[optind]);
        }
        else{
            sleepdelay(delay);
            if(push(coda_task_concorrente,data)==-1){
                perror("push");
            }
        }
        optind++;
    }
    //invio nella coda stringhe che indicano EOF così che i thread worker capiscano che non ci sono più file da estrarre dalla coda, invio tante stringhe di EOF quanti thread ho creato
    if(n_inserito!=-1){
   	for(int i=0; i<numero_thread; i++){
   	     if(push(coda_task_concorrente,"finito")==-1){
      	  	    perror("push");
      	  	    exit(EXIT_FAILURE);
  	     }
    	} 
    	//attesa dei thread
    	for(int i=0; i<numero_thread; i++){
        	if(pthread_join(threadpool[i],NULL)!=0){
        		perror("errore in pthread_join");
        		exit(EXIT_FAILURE);
        	}
    	}
    }
    else{
    	for(int i=0; i<N_THREAD; i++){
		if(push(coda_task_concorrente,"finito")==-1){
      	  	    exit(EXIT_FAILURE);
  	     }
    	} 
    	//attesa thread
    	for(int i=0; i<N_THREAD; i++){
        	if(pthread_join(threadpool[i],NULL)!=0){
        		perror("errore in pthread_join");
        		exit(EXIT_FAILURE);
        	}
    	}    
   }
   //I worker hanno finito il loro lavor, non ci sono più file da inviare ai worker, quindi posso chiudere la connessione e liberare la memoria occupata
    close(fd_c);
    free(Thread_Args);
    free(threadpool);
    deleteQueue(coda_task_concorrente);
}
