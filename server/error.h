#ifndef ERROR_H
#define ERROR_H
/* Error handler */

#include <stdio.h>
#include <time.h>
#include <pthread.h>

#include "utils.h"

extern time_t TIMET;
extern char LOG_FILE[];
extern char NO_TIME[];
extern pthread_mutex_t log_mutex;


/* when using ERROR, under 0 */
enum ERROR {
  E_ALL = -999,
  E_NULL,

  /* file system */
  E_NOT_EXIST,
  E_NOT_FILE,
  E_NOT_DIR,

  /* network */
  E_BIND,
  E_LISTEN,
  E_START_SOCKET,
  E_SOCKET_WRONG,

  /* socket */
  E_READ_NOTHING,
  E_READ_WRONG,
  E_READ_EOF,
  E_WRITE_WRONG,
  E_WRITE_INCOMP,
  E_OVER_BUFFER,

  /* ftp */
  E_NOT_UNDERSTAND,
  E_DSOCKET,
  E_THREAD,
  E_MSG_FAIL, // fail to gen a right msg
};

/* write log to LOG_FILE */
void logd(char* msg);

/* write log to LOG_FILE [error] */
void loge(char* msg);

/* write log to LOG_FILE [info] */
void logi(char* msg);

/* write log to LOG_FILE [warning] */
void logw(char* msg);

/* print out one pointer */
void ptrd(void*);

/* print out one char array */
void strd(char*);

/* print out one char array */
void intd(int);

/* get current time and gen a char array */
char* getTime(void);

#endif