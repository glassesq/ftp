#ifndef FILE_H
#define FILE_H

#include <stdlib.h>
#include <dirent.h>
#include <sys/stat.h>
#include "error.h"


/* check */

int checkDirectory(char*); 

int checkFile(char*);

int checkSub(char* child, char* parent);

#endif