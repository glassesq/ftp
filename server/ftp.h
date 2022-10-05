#ifndef FTP_H
#define FTP_H

#include <arpa/inet.h>
#include <netinet/in.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <unistd.h>

#include "config.h"

#define MAX_CONN 100

extern pthread_t threads[MAX_CONN];
extern int sk2th[MAX_CONN];

void init(void);

/* get a new thread id
 indicates that threads[rv] is free */
int getNewThreadId(void);

/* get its current thread[?] from socket id */
int socket2thread(int socket);

/* start server with argc and argv. */
void startServer(int argc, char* argv[]);

/* start listen to port with maxmium connection */
int startListen(int port, int connection);

/* make a new thread and handle socket things */
void runSocket(int socket);

/* good bye */
void bye(void);

#endif