#include "error.h"

char LOG_FILE[] = "./logfile.log";
char NO_TIME[] = "no timer available";
time_t TIMET;

void ptrd(void* x) { printf("[debug]ptr: %p\n", x); }

void strd(char* x) { printf("[debug]str: %s\n", x); }

void intd(int x) { printf("[debug]int: %d\n", x); }

char* getTime(void) {
  time(&TIMET);
  char* result = ctime(&TIMET);
  if (result == NULL) result = NO_TIME;
  removeChar(result, '\n');
  return result;
}

void logd(char* msg) {
  if (LOG_FILE == NULL || msg == NULL) return;
  FILE* fp = NULL;
  fp = fopen(LOG_FILE, "a+");
  char* current = getTime();
  fprintf(fp, "[%s][debug] %s\n", current, msg);
  printf("[%s][debug] %s\n", current, msg);
  fclose(fp);
}

void loge(char* msg) {
  if (LOG_FILE == NULL || msg == NULL) return;
  FILE* fp = NULL;
  fp = fopen(LOG_FILE, "a+");
  char* current = getTime();
  fprintf(fp, "[%s][error] %s\n", current, msg);
  printf("[%s][error] %s\n", current, msg);
  fclose(fp);
}

void logi(char* msg) {
  if (LOG_FILE == NULL || msg == NULL) return;
  FILE* fp = NULL;
  fp = fopen(LOG_FILE, "a+");
  char* current = getTime();
  fprintf(fp, "[%s][info] %s\n", current, msg);
  printf("[%s][info] %s\n", current, msg);
  fclose(fp);
}

void logw(char* msg) {
  if (LOG_FILE == NULL || msg == NULL) return;
  FILE* fp = NULL;
  fp = fopen(LOG_FILE, "a+");
  char* current = getTime();
  fprintf(fp, "[%s][warning] %s\n", current, msg);
  printf("[%s][warning] %s\n", current, msg);
  fclose(fp);
}
