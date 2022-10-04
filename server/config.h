#ifndef CONFIG_H
#define CONFIG_H

#include <stdio.h>

#include "file.h"
#include "error.h"

struct ServerConfig {
  int port;
  char* root;
};

/* config for FTP server */
extern struct ServerConfig config;

extern char DEFAULT_ROOT_PATH[];

/* check if confuration valid */
int checkConfig(void);

/* parse argument from startup */
int parseArgument(int argc, char* argv[]);

#endif