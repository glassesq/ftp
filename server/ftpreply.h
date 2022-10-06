#ifndef FTPREPLY_H
#define FTPREPLY_H

#include <stdlib.h>

#include "error.h"

enum FTPCode {
  CODE_ERROR = -1,
  CODE_OK = 200,
};

/* ftp reply struct */
struct reply {
  enum FTPCode code;
  char msg[BUFFER_SIZE];
};

extern struct reply REPLY125;
extern struct reply REPLY150;
extern struct reply REPLY220;
extern struct reply REPLY230;
extern struct reply REPLY331;
extern struct reply REPLY421;
extern struct reply REPLY500;
extern struct reply REPLY503;
extern struct reply REPLY530;
extern struct reply REPLY550E; /* not exist */
extern struct reply REPLY550P; /* no permission */
extern struct reply REPLY425;
extern struct reply REPLY226;

int genReply(struct reply* result, int code, char* data);

#endif