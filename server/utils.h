#ifndef UTILS_H
#define UTILS_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* transfer a string to int, save it to tar
   return value == 1 ? is int : not int.
*/
int str2int(char* src, int* tar);

/* transfer int to char array
   pointer can be safely used */
char* int2str(int src);

/* concat (former, latter) char array
  pointer can be safely used */
char* concat(char* former, char* latter);

/* remove target from char array src */
void removeChar(char* src, char target);

#endif