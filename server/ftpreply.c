#include "ftpreply.h"

struct reply REPLY220 = {.code = 220,
                         .msg = "FTP server ready for anonymous user\r\n"};

struct reply REPLY421 = {
    .code = 421,
    .msg = "Service not available, closing control connection.\r\n"};

struct reply REPLY331 = {.code = 331,
                         .msg =
                             "331 Guest login ok, send your complete e-mail "
                             "address as password.\r\n"};

struct reply REPLY230 = {.code = 230,
                         .msg = "WELCOME!\r\nguest login ok. Fantastic!\r\n"};

struct reply REPLY500 = {.code = 500,
                         .msg = "Syntax error, command unrecognized.\r\n"};

struct reply REPLY503 = {.code = 503, .msg = "Bad sequence of commands.\r\n"};

struct reply REPLY530 = {.code = 530, .msg = "Bad sequence of commands.\r\n"};

int genReply(struct reply* result, int code, char* data) {
  if (result == NULL) return E_NULL;
  result->code = code;
  if (strlen(data) >= BUFFER_SIZE) return E_OVER_BUFFER;
  strcpy(result->msg, data);
  return 1;
}