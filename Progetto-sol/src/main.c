#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <signal.h>
#include <master_worker.h>
#include <util.h>
#include <lista.h>
#include <collector.h>
#include <conn.h>
#define SOCKNAME "farm.sck"
#define UNIX_PATH_MAX 103
pthread_mutex_t lock_conn=PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t lock_term=PTHREAD_MUTEX_INITIALIZER;
int fd_c;
int termina;
//funzione che cancella il socket file
void cleanup(){
    unlink(SOCKNAME);
}
//task per il thread sighandel che gestisce le interruzzione come da specifica
static void *sigHandler(void *arg)
{
    sigset_t *set = (sigset_t*)arg;
    int sigusr1=-1;

    while(check_termina()!=-1)//ciclo while per ritornare in attesa di interruzioni nel caso in cui ci arrivi un'interruzione di tipo SIGUSR1
    {
		int sig;
		int r = sigwait(set, &sig);//aspetta che si verifichi una delle interruzioni settate nella mask

		if (r != 0) {
		    errno = r;
		    perror("FATAL ERROR 'sigwait'");
		    return NULL;
		}

		switch(sig)
		{
		case SIGINT:
		case SIGTERM:
		case SIGQUIT:
		case SIGHUP:
		    LOCK(&lock_term);	
		    termina = -1;
		    UNLOCK(&lock_term);
		    break;
		case SIGUSR1:
		    LOCK(&lock_conn);
		    printf("Ricevuto interruzione SIGUSR1\n");
		    printf("File ricevuti dal collector fino a questo momento: \n");
		    SYSCALL_EXIT("write 1 master",r,writen(fd_c,&sigusr1, sizeof (int)),"write lunghezza","")
		    UNLOCK(&lock_conn);
		    break;
		default:
		 	break; 
		}
    }
       
    return NULL;	   
}
int main(int argc, char* argv[]) {
    cleanup();//se il processo era stato bloccato da un'interruzione che non gestiamo, il main elimina il precedente socket file
    sigset_t mask, oldmask;
    sigemptyset(&mask);
    sigaddset(&mask, SIGINT);
    sigaddset(&mask, SIGQUIT);
    sigaddset(&mask, SIGHUP);
    sigaddset(&mask, SIGTERM);
    sigaddset(&mask, SIGUSR1);
    //creazione del thread per le interruzioni
    if (pthread_sigmask(SIG_BLOCK, &mask, &oldmask) == -1)
    {
        perror("pthread_sigmask");
        exit(EXIT_FAILURE);
    }

    struct sigaction s;
    memset(&s, 0, sizeof(s));
    s.sa_handler = SIG_IGN;
    if ((sigaction(SIGPIPE, &s, NULL)) == -1)
    {
        perror("sigaction");
        exit(EXIT_FAILURE);
    }
    pthread_t sighandler_thread;
    if(pthread_create(&sighandler_thread, NULL, sigHandler, &mask) != 0)
    {
		fprintf(stderr, "errore nella creazione del signal handler thread\n");
		abort();
    }
    int fd_skt;
    int status;
    struct sockaddr_un sa;
    strncpy(sa.sun_path,SOCKNAME,UNIX_PATH_MAX);
    sa.sun_family=AF_UNIX;
    int pid;
    int not_used;
    pid=fork();
    //processo figlio chiama la funzione del collector
    if(pid==0){
        collector_process();
    }
    else{
    	//creazione della connessione e si rimane in attesa fino a quando il preccosso collector si colleghi
    	SYSCALL_EXIT("socket",fd_skt,socket(AF_UNIX,SOCK_STREAM,0),"socket","");
        SYSCALL_EXIT("bind",not_used,bind(fd_skt,(struct sockaddr *)&sa,sizeof (sa)),"bind","");
	SYSCALL_EXIT("listen",not_used,listen(fd_skt,SOMAXCONN),"listen","");
	SYSCALL_EXIT("accept",fd_c,accept(fd_skt,NULL,0),"accept","");
	//il thread main esegue la funzione master_worker che legge i file in input e crea il pool di thread worker
        master_worker(argc,argv,fd_c);
        //si resta in attesa del processo collector
        if(waitpid(pid,&status,0)==-1){
        	perror("errore in waitpid");
        	cleanup();
        	exit(EXIT_FAILURE);
        }
        pthread_cancel(sighandler_thread);
    	if ((pthread_join(sighandler_thread, NULL)) != 0)
    	{
       	 perror("errore in pthread_join");
       	 cleanup();
       	 exit(EXIT_FAILURE);
    	}
    	//prima di terminare tutto elimino il socket file
    	cleanup();
    }

    return 0;
}
