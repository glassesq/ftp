#ifndef CONFIG_H
#define CONFIG_H

#include <stdio.h>
#include <unistd.h>

#include "error.h"
#include "file.h"
#define IP_SIZE 15

struct ServerConfig {
  int port;
  char* root;
  int max_connect;   // max connection at the same time
  char ip[IP_SIZE];  // ip address for the server
};

/* config for FTP server */
extern struct ServerConfig config;

extern char DEFAULT_ROOT_PATH[];

/* check if confuration valid */
int checkConfig(void);

/* parse argument from startup */
int parseArgument(int argc, char* argv[]);

#endif