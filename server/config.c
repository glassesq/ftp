#include "config.h"

#include "error.h"

char DEFAULT_ROOT_PATH[] = "/tmp";

struct ServerConfig config = {.port = 21, .root = DEFAULT_ROOT_PATH};

int parseArgument(int argc, char* argv[]) {
  int cnt = 1;
  while (cnt < argc) {
    if (strcmp(argv[cnt], "-port") == 0) {
      if (cnt >= argc - 1 || !str2int(argv[cnt + 1], &(config.port)) ||
          config.port < 0) {
        loge("invalid argument: port number");
        return 0;
      }
      intd(config.port);
      logd(concat("port number set to ", int2str(config.port)));
      cnt += 2;
    } else if (strcmp(argv[cnt], "-root") == 0) {
      if (cnt >= argc - 1 || checkDirectory(argv[cnt + 1])) {
        loge("invalid argument: root directory");
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
  // TODO
  if (config.port < 0) return -1;

  return 0;
}
