#include "file.h"

int checkDirectory(char* path) {
  if (path == NULL) return E_NULL;
  struct stat buf;
  if (stat(path, &buf) < 0) return E_NOT_EXIST;
  if (S_ISDIR(buf.st_mode) == 0) return E_NOT_DIR;
  return 1;
}

int checkFile(char* path) {
  if (path == NULL) return E_NULL;
  struct stat buf;
  if (stat(path, &buf) < 0) return E_NOT_EXIST;
  if (S_ISREG(buf.st_mode) == 0) return E_NOT_FILE;
  return 1;
}

int checkSub(char* child, char* parent) {
  char* cr = realpath(child, NULL);
  char* pr = realpath(parent, NULL);
  if (startswith(cr, pr)) return 1;
  return 0;
}
