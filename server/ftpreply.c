#include "ftpreply.h"

struct reply REPLY125 = {
    .code = 125, .msg = "Data connection already open; transfer starting.\r\n"};

struct reply REPLY150 = {
    .code = 150,
    .msg = "File status okay\r\nAbout to open data connection.\r\n"};

struct reply REPLY150D = {
    .code = 150,
    .msg = "Directory info okay\r\nAbout to open data connection.\r\n"};

struct reply REPLY215 = {.code = 215, .msg = "UNIX Type: L8\r\n"};

struct reply REPLY220 = {.code = 220,
                         .msg = "FTP server ready for anonymous user\r\n"};

struct reply REPLY221 = {
    .code = 221, .msg = "Thank you for using the FTP service\r\nGood bye.\r\n"};

struct reply REPLY421 = {
    .code = 421,
    .msg = "Service not available, closing control connection.\r\n"};

struct reply REPLY331 = {
    .code = 331,
    .msg =
        "Guest login ok, send your complete e-mail address as password.\r\n"};

struct reply REPLY350 = {
    .code = 350,
    .msg = "Requested file action pending further information.\r\n"};

struct reply REPLY230 = {.code = 230,
                         .msg = "WELCOME!\r\nguest login ok. Fantastic!\r\n"};

struct reply REPLY250 = {.code = 250,
                         .msg = "Requested file action okay, completed.\r\n"};

struct reply REPLY425 = {
    .code = 425,
    .msg =
        "Can not open data connection.\r\nNo established TCP connection.\r\n"};

struct reply REPLY426 = {.code = 426,
                         .msg = "Connection closed; transfer aborted.\r\n"};

struct reply REPLY500 = {.code = 500,
                         .msg = "Syntax error, command unrecognized.\r\n"};

struct reply REPLY501 = {.code = 501,
                         .msg = "Syntax error in parameters or arguments.\r\n"};

struct reply REPLY503 = {.code = 503, .msg = "Bad sequence of commands.\r\n"};

struct reply REPLY530 = {.code = 530, .msg = "Not logged in.\r\n"};

struct reply REPLY550 = {.code = 550, .msg = "Requested action not taken.\r\n"};

struct reply REPLY550E = {
    .code = 550, .msg = "Requested action not taken.\r\nFile not found.\r\n"};

struct reply REPLY550D = {
    .code = 550,
    .msg = "Requested action not taken.\r\nDirectory not empty.\r\n"};

struct reply REPLY550P = {
    .code = 550, .msg = "Requested action not taken.\r\nNo access.\r\n"};

struct reply REPLY226 = {
    .code = 226,
    .msg = "Closing data connection.\r\nRequested file action successful\r\n"};

int genReply(struct reply* result, int code, char* data) {
  if (result == NULL) return E_NULL;
  result->code = code;
  if (strlen(data) >= BUFFER_SIZE) return E_OVER_BUFFER;
  strcpy(result->msg, data);
  return 1;
}