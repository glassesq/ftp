#ifndef ERROR_H
#define ERROR_H
/* Error handler */

#include <stdio.h>
#include <time.h>

#include "utils.h"

extern time_t TIMET;
extern char LOG_FILE[];
extern char NO_TIME[];

/* when using ERROR, under 0 */
enum ERROR { E_ALL = -999 };

/* write log to LOG_FILE */
void logd(char* msg);

/* write msg on screen */
void loge(char* msg);

/* print out one pointer */
void ptrd(void*);

/* print out one char array */
void strd(char*);

/* print out one char array */
void intd(int);

/* get current time and gen a char array */
char* getTime(void);

/* remove every target char from char* */
void removeChar(char*, char);

#endif