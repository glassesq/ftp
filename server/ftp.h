#ifndef FTP_H
#define FTP_H

#include <arpa/inet.h>
#include <ctype.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <unistd.h>

#include "config.h"
#include "ftpreply.h"

#define MAX_CONN 100

extern pthread_t threads[MAX_CONN];
extern int sk2th[MAX_CONN];
extern char socket_buffer[BUFFER_SIZE];

enum FTPMode {
  FTP_M_ERROR = -1,
  FTP_M_PASV_LISTEN,  // now listen to tcp
  FTP_M_PASV_ACCEPT,  // accept tcp, waiting command
  FTP_M_EMPTY,        // empty
};

enum FTPType {
  FTP_ERROR = -1,
  FTP_USER = 0,
  FTP_PASS,
  FTP_PASV,
  FTP_RETR,
  FTP_QUIT,
  FTP_ABOR,
  FTP_LIST,
  FTP_TYPE,
  FTP_SYST,
};

struct conn_info {
  int id;
  enum FTPMode mode;  // TODO: lock required
  int dserver_socket;
  int d_socket;
  pthread_t d_thread;
  char work_dir[BUFFER_SIZE];
};

struct request {
  enum FTPType type;
  char params[BUFFER_SIZE];
};

void init(void);

/* get a new thread id
 indicates that threads[rv] is free */
int getNewThreadId(void);

/* get its current thread[?] from socket id */
int socket2thread(int sock);

/* start server with argc and argv. */
void startServer(int argc, char* argv[]);

/* start listen to port with maxmium connection */
int startListen(int port, int connection);

/* read at least one request from buffer to result in str format
- if buffer is empty, read from/wait for socket
- NULL for error*/
int readOneRequest(int sock, char* result);

/* read everything currently in socket, beacuse of a error */
void clearSocket(int sock);

/* send reply to socket */
int sendReply(int sock, struct reply r);

/* make a new thread and handle socket things */
void runSocket(int sock);

/* process the raw str, remove [...]USER[...]parameter[\r\n] */
void processRaw(char* raw);

/* see if there is a greeting */
int greeting(int sock);

/* socket is ok, now run FTP procedure */
void runFTP(int ftp_socket, struct conn_info* info);

/* write raw to sock */
int writeRaw(int sock, char* raw);

/* write raw to sock */
int writeFile(int sock, char* path);

/* parse a raw request to get a struct req */
int parseRawRequest(char* raw, struct request* req);

/* handle login in socket with (login)req and info */
int handleLogin(int ftp_socket, struct request req, struct conn_info* info);

/* handle RETR command */
int handleRetr(int ftp_socket, struct request req, struct conn_info* info);

/* handle PASV mode */
int handlePasv(int ftp_socket, struct request req, struct conn_info* info);

/* handle TYPE (I) only */
int handleType(int ftp_socket, struct request req, struct conn_info* info);

/* handle SYST */
int handleSyst(int ftp_socket, struct request req, struct conn_info* info);

/* handle QUIT and quit*/
int handleQuit(int ftp_socket, struct request req, struct conn_info* info);

/* handle ABOR and quit */
int handleAbor(int ftp_socket, struct request req, struct conn_info* info);

/* new socket and bind to port */
int newBindSocket(int port, char* address);

/* handle unexpectely pass command */
int handleUnexpPass(int const, struct request req, struct conn_info* info);

/* check if it has logged in. */
int checkLogin(int ftp_socket, struct conn_info* info);

/* good bye */
void bye(void);

/* sync run FTP main procedure */
void* syncRun(void* socket_ptr);

/* sync run FTP data procedure */
void* syncData(void* info_ptr);

/* clear data connection, not included the ftp_socket itself */
void clearConn(struct conn_info* info);

#endif