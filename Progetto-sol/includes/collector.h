#ifndef COLLECTOR_H_
#define COLLECTOR_H_
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/un.h>
#define SOCKNAME "farm.sck"
#define UNIX_PATH_MAX 103
void collector_process();

#endif
