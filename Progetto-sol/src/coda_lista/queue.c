#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <errno.h>
#include <pthread.h>
#include <queue.h>
#include <util.h>
/**
 * @file queue.c
 * @brief File di implementazione dell'interfaccia per la coda
 */



/* ------------------- funzioni di utilita' -------------------- */

// qui assumiamo (per semplicita') che le mutex non siano mai di
// tipo 'robust mutex' (pthread_mutex_lock(3)) per cui possono
// di fatto ritornare solo EINVAL se la mutex non e' stata inizializzata.

static inline Node_t*  allocNode()                  { return malloc(sizeof(Node_t));  }
static inline Queue_t* allocQueue()                 { return malloc(sizeof(Queue_t)); }
static inline void freeNode(Node_t *node)           { free((void*)node); }
static inline void LockQueue(Queue_t *q)            { LOCK(&q->qlock);   }
static inline void UnlockQueue(Queue_t *q)          { UNLOCK(&q->qlock); }
static inline void UnlockQueueAndWait_piena(Queue_t *q)   { WAIT(&q->coda_piena, &q->qlock);}
static inline void UnlockQueueAndWait_vuota(Queue_t *q)    { WAIT(&q->coda_vuota,&q->qlock);}
static inline void UnlockQueueAndSignal_piena(Queue_t *q) { SIGNAL(&q->coda_piena); UNLOCK(&q->qlock);}
static inline void UnlockQueueAndSignal_vuota(Queue_t *q) { SIGNAL(&q->coda_vuota); UNLOCK(&q->qlock);}

/* ------------------- interfaccia della coda ------------------ */

Queue_t *initQueue(long max_leng)
{
    Queue_t *q = allocQueue();

    if(!q)
        return NULL;

    q->head = allocNode();
    if(!q->head)
        return NULL;
    q->head->data = NULL;
    q->head->next = NULL;

    q->tail = q->head;
    q->qlen = 0;
    q->maxlen=max_leng;
    if(pthread_mutex_init(&q->qlock, NULL) != 0)
    {
        perror("mutex init");
        return NULL;
    }

    if(pthread_cond_init(&q->coda_piena, NULL) != 0)
    {
        perror("mutex coda_piena");
        if(&q->qlock)
            pthread_mutex_destroy(&q->qlock);
        return NULL;
    }
    if(pthread_cond_init(&q->coda_vuota,NULL)!=0){
        perror("mutex coda_vuota");
        if(&q->qlock){
            pthread_mutex_destroy(&q->qlock);
        }
        return NULL;
    }
    return q;
}

void deleteQueue(Queue_t *q)
{
    while(q->head != q->tail)
    {
        Node_t *p = q->head;
        q->head = q->head->next;
        freeNode(p);
    }

    if(q->head)
        freeNode(q->head);
    if(&q->qlock)
        pthread_mutex_destroy(&q->qlock);
    if(&q->coda_piena)
        pthread_cond_destroy(&q->coda_piena);
    if(&q->coda_vuota)
        pthread_cond_destroy(&q->coda_vuota);
    free(q);
}

int push(Queue_t *q, char *data)
{
    if((q == NULL) || (data == NULL))
    {
        errno = EINVAL;
        return -1;
    }

    Node_t *n = allocNode();

    if(!n)
        return -1;
    n->data = data;
    n->next = NULL;

    LockQueue(q);
    while (q->qlen==q->maxlen){
        UnlockQueueAndWait_piena(q);
    }
    q->tail->next = n;
    q->tail = n;
    q->qlen += 1;
    UnlockQueueAndSignal_vuota(q);
    return 0;
}

void* pop(Queue_t *q)
{
    if(q == NULL)
    {
        errno = EINVAL;
        return NULL;
    }
    LockQueue(q);
    while(q->qlen==0)
    {
        UnlockQueueAndWait_vuota(q);
    }
    // locked
    assert(q->head->next);
    Node_t *n = (Node_t*)q->head;
    void *data = (q->head->next)->data;
    q->head = q->head->next;
    q->qlen -= 1;
    assert(q->qlen >= 0);
    UnlockQueueAndSignal_piena(q);
    UnlockQueue(q);
    freeNode(n);
    return data;
}

// NOTA: in questa funzione si puo' accedere a q->qlen NON in mutua esclusione
//       ovviamente il rischio e' quello di leggere un valore non aggiornato, ma nel
//       caso di piu' produttori e consumatori la lunghezza della coda per un thread
//       e' comunque un valore "non affidabile" se non all'interno di una transazione
//       in cui le varie operazioni sono tutte eseguite in mutua esclusione.
unsigned long length(Queue_t *q)
{
    LockQueue(q);
    unsigned long len = q->qlen;
    UnlockQueue(q);

    return len;
}
