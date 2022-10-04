#include "utils.h"

int str2int(char* src, int* tar) {
  if (src == NULL) return 0;
  removeChar(src, ' ');

  int len = strlen(src);
  if (len == 0) return 0;

  int cnt = 0;
  int sign = 1;
  while (cnt < len && src[cnt] == '-') {
    sign = -sign;
    cnt++;
  }

  int value = 0;
  while (cnt < len) {
    if (src[cnt] > '9' || src[cnt] < '0') return 0;
    value = value * 10 + (int)(src[cnt] - '0');
    ++cnt;
  }

  if (tar == NULL) return 1;
  *tar = value * sign;

  return 1;
}

char* int2str(int src) {
  char* result = malloc(sizeof(char) * 15);
  sprintf(result, "%d", src);
  return result;
}

char* concat(char* former, char* latter) {
  if (former == NULL || latter == NULL) return NULL;
  char* result = malloc(sizeof(char) * (strlen(former) + strlen(latter) + 1));
  strcpy(result, former);
  strcat(result, latter);
  return result;
}

void removeChar(char* src, char target) {
  if (src == NULL) return;
  int len = strlen(src);
  int cnt = 0;
  for (int i = 0; i < len; i++) {
    if (src[i] == target) continue;
    src[cnt] = src[i];
    ++cnt;
  }
  src[cnt] = '\0';
}
