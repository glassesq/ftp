#ifndef UTILS_H
#define UTILS_H

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define BUFFER_SIZE 8192
// PATH_MAX is 4096

extern char formatb[BUFFER_SIZE];

/* remove the first [target]* from raw */
void removeFirstSec(char* raw, char target);

/* find out if it starts with target */
int startswith(char* raw, char* target);

/* write log to LOG_FILE [warning] */
void logw(char* msg);

/* transfer a string to int, save it to tar
   return value == 1 ? is int : not int.
*/
int str2int(char* src, int* tar);

/* transfer int to char array
   pointer can be safely used */
char* int2str(int src);

/* concat (former, latter) char array
  pointer can be safely used
  [Warning] : memory LEAK here!
*/
char* concat(char* former, char* latter);

/* remove target from char array src */
void removeChar(char* src, char target);

/* remove the very FIRST char from char array src */
void removeFirstChar(char* src, char target);

/* format a str printf-like to formatd
   return value are fixed to global char array ptr formatb[].

 */
char* formatstr(char* format, ...);

#endif