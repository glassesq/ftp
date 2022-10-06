#include "utils.h"

char formatb[BUFFER_SIZE];

/* it is OK to use atoi, HOWEVER 0 may be confused */
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

char* formatstr(char* format, ...) {
  va_list args;
  va_start(args, format);
  int result = vsnprintf(formatb, BUFFER_SIZE, format, args);
  if (result + 1 > BUFFER_SIZE) logw("overflow buffer when format str");
  return formatb;
}

void removeFirstSec(char* raw, char target) {
  if (raw == NULL) return;
  int cnt = 0, len = strlen(raw);
  int flag = 0;
  for (int i = 0; i < len; i++) {
    if (raw[i] != target) {
      if (i > 0 && raw[i - 1] == target) flag = 1;
      raw[cnt] = raw[i];
      ++cnt;
    } else if (flag) {
      raw[cnt] = raw[i];
      ++cnt;
    }
  }
  raw[cnt] = '\0';
}

void removeFirstChar(char* src, char target) {
  if (src == NULL) return;
  int len = strlen(src);
  int cnt = 0, flag = 0;
  for (int i = 0; i < len; i++) {
    if (!flag && src[i] == target) {
      flag = 1;
      continue;
    }
    src[cnt] = src[i];
    ++cnt;
  }
  src[cnt] = '\0';
}

int startswith(char* raw, char* target) {
  if( raw == NULL || target == NULL) return 0;
  return strncmp(raw, target, strlen(target)) == 0;
}