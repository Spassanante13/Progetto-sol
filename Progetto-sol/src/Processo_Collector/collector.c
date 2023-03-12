#include <errno.h>
#include <collector.h>
#include <util.h>
#include <lista.h>
#include <conn.h>
void collector_process(){
    int fd_skt;
    struct sockaddr_un sa;
    int liung;
    long ris=0;
    int r;
    ListNodePtr startptr=NULL;
    strncpy(sa.sun_path,SOCKNAME,UNIX_PATH_MAX);
    sa.sun_family=AF_UNIX;
    fd_skt= socket(AF_UNIX,SOCK_STREAM,0);
    //avvio la connessione con il processo Master
    while (connect(fd_skt,(struct sockaddr *)&sa,sizeof (sa))==-1) {
        if (errno == ENOENT) {
            sleep(1);
        } else {
            perror("problemi di connessione");
            exit(EXIT_FAILURE);
        }
    }
    char buf[256];
    int lung_read;
    //la read è un'operazione bloccante, quindi attendo che i worker mi inviino un intero che rappresenta la lunghezza della stringa  da leggere
    SYSCALL_EXIT("read 1 collector",lung_read, readn(fd_skt,&liung,sizeof (int)),"read1","");
    //rimango in questo ciclo fino a quando la read mi restituisce un valore positivo (la connessione non è chiusa)
    while(lung_read > 0){
    //liung=-1 se e solo se il threas sighandler_thread ha ricevuto il segnale SIGUSR1 e quindi il collector deve stampare quello che ha ricevuto fino a quel momento
    	if(liung==-1){
    		printList(startptr);
    		printf("continua ricezione file dai thread worker\n");
    		SYSCALL_EXIT("read 1 collector",lung_read, readn(fd_skt,&liung,sizeof (int)),"read1","");
    	}
    	else{
    	//per ogni read comunico al thread worker se è andata a buon fine e che può procedere con la prossima write
           SYSCALL_EXIT("write 1 collector",r, writen(fd_skt,"letto",5),"write1","")
           SYSCALL_EXIT("read 2 collector",r, readn(fd_skt,buf,liung*sizeof (char )),"read2","")
           SYSCALL_EXIT("write 2 collector",r, writen(fd_skt,"letto",5),"write2","")
           SYSCALL_EXIT("read 3 collector",r, readn(fd_skt,&ris,sizeof (long )),"read3","")
           SYSCALL_EXIT("write 3 collector",r, writen(fd_skt,"letto",5),"write3","")
           //inserimento in una lista ordinata di ciò che si è ricevuto
           insert(&startptr,buf,ris);
           SYSCALL_EXIT("read 1 collector",lung_read, readn(fd_skt,&liung,sizeof (int)),"read1","");
        }
    }
    printList(startptr);
    libera(&startptr);
    unlink(SOCKNAME);
}
