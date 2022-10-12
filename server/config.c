#include "config.h"

#include "error.h"

char DEFAULT_ROOT_PATH[] = "/tmp";

struct ServerConfig config = {
    .port = 21, .root = DEFAULT_ROOT_PATH, .max_connect = 1, .ip = "0.0.0.0"};

int parseArgument(int argc, char* argv[]) {
  int cnt = 1, ret;
  while (cnt < argc) {
    if (strcmp(argv[cnt], "-port") == 0) {
      if (cnt >= argc - 1 || !str2int(argv[cnt + 1], &(config.port)) ||
          config.port < 0) {
        loge("invalid argument: port number");
        return 0;
      }
      cnt += 2;
    } else if (strcmp(argv[cnt], "-root") == 0) {
      if (cnt >= argc - 1) {
        loge("invalid argument: missing root path");
        return 0;
      }
      config.root = realpath(argv[cnt + 1], NULL);
      if (config.root == NULL)
        loge("invalid argument: root directory not exist");
      if ((ret = checkDirectory(config.root)) < 0) {
        if (ret == E_NOT_DIR) loge("invalid argument: root is not a directory");
        return 0;
      }
      cnt += 2;
    } else {
      loge("invalid argument: not understand");
      return 0;
    }
  }
  return 1;
}

int checkConfig(void) {
  if (config.port < 0) return -1;
  if (checkDirectory(config.root) < 0) return E_NOT_EXIST;
  return 1;
}
