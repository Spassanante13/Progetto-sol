#ifndef WORKER_H_
#define WORKER_H_
struct thread_arg{
    int fd_c;
    Queue_t *coda_task;
};
void* threadfunc(void* arg);

#endif
