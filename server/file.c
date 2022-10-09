#include "file.h"

int checkDirectory(char* path) {
  if (path == NULL) return E_NULL;
  if (path[0] != '/') {
    loge(formatstr("[%s] is not an absolute path", path));
  }
  struct stat buf;
  if (stat(path, &buf) < 0) return E_NOT_EXIST;
  if (S_ISDIR(buf.st_mode) == 0) return E_NOT_DIR;
  return 1;
}

int checkFile(char* path) {
  if (path == NULL) return E_NULL;
  if (path[0] != '/') {
    loge(formatstr("[%s] is not an absolute path", path));
  }
  struct stat buf;
  if (stat(path, &buf) < 0) return E_NOT_EXIST;
  if (S_ISREG(buf.st_mode) == 0) return E_NOT_FILE;
  return 1;
}

int checkSub(char* child, char* parent) {
  if (child[0] != '/' || parent[0] != '/') {
    loge(formatstr("[%s]or[%s] is not an absolute path", child, parent));
  }
  if (startswith(child, parent)) return 1;
  return 0;
}
