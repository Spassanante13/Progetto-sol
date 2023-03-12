#ifndef LISTA_H_
#define LISTA_H_
#include<stdio.h>
#include <stdlib.h>
struct listNode{
    long risultato;
    char stringa[256];
    struct listNode *nextPtr;
};
typedef struct listNode ListNode;
typedef ListNode *ListNodePtr;
void insert(ListNodePtr *sptr, char* string, long risultato);
int isEmpty(ListNodePtr sptr);
void libera(ListNodePtr *sptr);
void printList(ListNodePtr currentPtr);

#endif
