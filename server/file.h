#ifndef FILE_H
#define FILE_H

#include <dirent.h>
#include <grp.h>
#include <math.h>
#include <pwd.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/types.h>

#include "error.h"

/* check */
int checkDirectory(char*);

int checkFile(char*);

/*
check if child is a subfile/dir of parent.
A is A's sub.
*/
int checkSub(char* child, char* parent);

#endif