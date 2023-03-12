#include <string.h>
#include <stdlib.h>
#include <lista.h>
void insert(ListNodePtr *sptr, char* string, long risultato){
    ListNodePtr newptr= malloc(sizeof (ListNode));
    if(newptr!=NULL){
        newptr->risultato=risultato;
        strncpy(newptr->stringa,string,256);
        newptr->nextPtr=NULL;
        ListNodePtr previousPtr=NULL;
        ListNodePtr currentPtr=*sptr;
        //scrorro la lista fino a quando il risultato del nuovo nodo da inserire è maggiore del risultato del nodo corrente
        while(currentPtr!=NULL && risultato > currentPtr->risultato){
            previousPtr=currentPtr;
            currentPtr=currentPtr->nextPtr;
        }
        if(previousPtr==NULL){
            newptr->nextPtr=*sptr;
            *sptr=newptr;
        } else{
            previousPtr->nextPtr=newptr;
            newptr->nextPtr=currentPtr;
        }
    } else{
        printf("NOn inserito\n");
    }
}
int isEmpty(ListNodePtr sptr){
    return sptr==NULL;
}
void printList(ListNodePtr currentptr){
    if(isEmpty(currentptr)){
        printf("La lista è vuota\n");
    }
    else{
    //fino a quando non mi trovo all'utlimo nodo della lista, stampo risultato e stringa.
        while(currentptr!=NULL){
            printf("%ld   %s\n",currentptr->risultato,currentptr->stringa);
            currentptr=currentptr->nextPtr;
        }
    }
}
void libera(ListNodePtr *sptr){
	ListNodePtr tempptr=NULL;
	while(*sptr!=NULL){
		tempptr=*sptr;
		*sptr=(*sptr)->nextPtr;
		free(tempptr);
	}
}
