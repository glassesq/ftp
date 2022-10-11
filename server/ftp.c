#include "ftp.h"

pthread_mutex_t dir_mutex;
pthread_t threads[MAX_CONN];
int sk2th[MAX_CONN];
char socket_buffer[BUFFER_SIZE];

void startServer(int argc, char* argv[]) {
  logi("[WELCOME]welcome to server");
  if (parseArgument(argc, argv) <= 0 || checkConfig() <= 0) {
    bye();
    return;
  }
  int ret = chdir(config.root);
  if (ret < 0) {
    loge(formatstr("change dir to config.root failed %s", config.root));
    return;
  }
  ret = startListen(config.port, config.max_connect);
  if (ret < 0) loge("listen failed");
}

void bye(void) { logi("The server is down. Good Bye!"); }

void init(void) {
  for (int i = 0; i < MAX_CONN; i++) sk2th[i] = -1;
}

int newBindSocket(int port, char* address) {
  if (address == NULL) {
    loge("address is NULL when try to make a new socket & bind it");
    return E_BIND;
  }

  // TODO: use the real address
  logw(formatstr("address [%s] provided, not using it now.", address));

  int s = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
  if (s < 0) {
    loge("failed to start socket");
    return E_START_SOCKET;
  }

  /* set server's address */
  struct sockaddr_in addr;
  memset(&addr, 0, sizeof(addr));
  addr.sin_family = AF_INET;
  if (port < 0) {
    logw(formatstr("port [%d] provided not ok, use any avaialble here", port));
  } else
    addr.sin_port = htons(port);
  addr.sin_addr.s_addr = htonl(INADDR_ANY);  // inet_addr("127.0.0.1")
  logd("addr generated");

  /* bind server to its address */
  int ret = bind(s, (struct sockaddr*)&addr, sizeof(addr));
  if (ret < 0) {
    loge(formatstr("failed to bind to %d", port));
    return E_BIND;
  }
  logd(formatstr("new socket %d made. bind to port [%d](not-for-sure)", s,
                 ntohs(addr.sin_port)));
  return s;
}

int startListen(int port, int connection) {
  init();

  /* create the socket */
  int server_socket;
  int ret = newBindSocket(port, config.ip);
  if (ret < 0) {
    loge("server socket bind failed");
    return E_BIND;
  } else
    server_socket = ret;

  /* bind server to its address */
  logi(formatstr("server_socket is ok. it is socket %d", server_socket));

  /* start listening */
  if ((ret = listen(server_socket, connection)) < 0) {
    loge("cannot listen");
    return E_LISTEN;
  }
  logi(formatstr("listen ok... listening at port %d", port));

  while (1) {
    int new_socket = accept(server_socket, NULL, NULL);
    intd(new_socket);
    logi("some one knock the door, we try to accept it");
    if (new_socket < 0) {
      loge("failed to accept connection");
      continue;
    }
    logi("accept ok");
    runSocket(new_socket);
  }
}

int getNewThreadId(void) {
  for (int i = 0; i < MAX_CONN; i++) {
    if (sk2th[i] == -1) return i;
  }
  return -1;
}

int socket2thread(int sock) {
  for (int i = 0; i < MAX_CONN; i++) {
    if (sk2th[i] == sock) return i;
  }
  return -1;
}

int readOneRequest(int sock, char* result) {
  int count = 0, former_r = 0;
  while (1) {
    int n = read(sock, result + count, 1);
    if (n == 0) {
      logd("read the EOF end but no \\r\\n here");
      return E_READ_EOF;
    }
    if (n < 0) {
      logd("read go wrong, mostly beacuse the sock is closed");
      return E_READ_WRONG;
    }

    if (former_r && result[count] == '\n') {
      result[count + 1] = '\0';
      logd("one new request read");
      return 1;
    }

    if (result[count] == '\r')
      former_r = 1;
    else
      former_r = 0;

    // TODO: illegal character check
    if (result[count] == '\0') continue;
    count += n;

    /* need to read next char, but there is no space for '\0'(additonal +1) */
    if (count + 1 >= BUFFER_SIZE) {
      return E_OVER_BUFFER;
    }
  }
}

int sendReply(int sock, struct reply r) {
  char raw[BUFFER_SIZE * 4];

  /* gen reply raw */
  int newline = 1, lastd = -1, cnt = 0;
  for (int i = 0; i < (int)strlen(r.msg); i++) {
    if (newline) {
      raw[cnt] = (char)(r.code / 100 + '0');
      raw[cnt + 1] = (char)((r.code % 100) / 10 + '0');
      raw[cnt + 2] = (char)(r.code % 10 + '0');
      raw[cnt + 3] = '-';
      lastd = cnt + 3;
      cnt = cnt + 4;
      newline = 0;
    }
    raw[cnt] = r.msg[i];
    cnt = cnt + 1;
    if (i >= 1 && r.msg[i] == '\n' && r.msg[i - 1] == '\r') {
      newline = 1;
    }
  }
  raw[cnt] = '\0';
  raw[lastd] = ' ';

  return writeRaw(sock, raw);
}

void processRaw(char* raw) {
  if (raw == NULL) return;
  int len = strlen(raw);
  if (raw[len - 1] == '\n' && raw[len - 2] == '\r') {
    raw[len - 2] = '\0';
    len -= 2;
  }
  if (raw[0] == ' ') removeFirstSec(raw, ' ');
  removeFirstChar(raw, ' ');
  logd(formatstr("raw information: [%s]", raw));
}

int parseRawRequest(char* raw, struct request* req) {
  if (req == NULL || raw == NULL) return E_NULL;
  processRaw(raw);
  /* 4-letter */
  if (startswith(raw, "USER")) {
    req->type = FTP_USER;
    strcpy(req->params,
           raw + 4);  // +1 for the extra " " between command and parameter
    return 1;
  } else if (startswith(raw, "PASS")) {
    req->type = FTP_PASS;
    strcpy(req->params, raw + 4);
    return 1;
  } else if (startswith(raw, "PORT")) {
    req->type = FTP_PORT;
    strcpy(req->params, raw + 4);
    return 1;
  } else if (startswith(raw, "PASV")) {
    req->type = FTP_PASV;
    strcpy(req->params, raw + 4);
    return 1;
  } else if (startswith(raw, "RETR")) {
    req->type = FTP_RETR;
    strcpy(req->params, raw + 4);
    return 1;
  } else if (startswith(raw, "SYST")) {
    req->type = FTP_SYST;
    strcpy(req->params, raw + 4);
    return 1;
  } else if (startswith(raw, "TYPE")) {
    req->type = FTP_TYPE;
    strcpy(req->params, raw + 4);
    return 1;
  } else if (startswith(raw, "QUIT")) {
    req->type = FTP_QUIT;
    strcpy(req->params, raw + 4);
    return 1;
  } else if (startswith(raw, "ABOR")) {
    req->type = FTP_ABOR;
    strcpy(req->params, raw + 4);
    return 1;
  } else if (startswith(raw, "LIST")) {
    req->type = FTP_LIST;
    strcpy(req->params, raw + 4);
    return 1;
  } else if (startswith(raw, "MKD")) {
    req->type = FTP_MKD;
    strcpy(req->params, raw + 3);
    return 1;
  } else if (startswith(raw, "CWD")) {
    req->type = FTP_CWD;
    strcpy(req->params, raw + 3);
    return 1;
  } else if (startswith(raw, "RMD")) {
    req->type = FTP_RMD;
    strcpy(req->params, raw + 3);
    return 1;
  } else if (startswith(raw, "PWD")) {
    req->type = FTP_PWD;
    strcpy(req->params, raw + 3);
    return 1;
  } else if (startswith(raw, "RNFR")) {
    req->type = FTP_RNFR;
    strcpy(req->params, raw + 4);
    return 1;
  } else if (startswith(raw, "RNTO")) {
    req->type = FTP_RNTO;
    strcpy(req->params, raw + 4);
    return 1;
  }
  return E_NOT_UNDERSTAND;
}

int writeRaw(int sock, char* raw) {
  if (raw == NULL) return E_NULL;
  int len = strlen(raw);  // the length of raw should be lower than 0x7fff0000
                          // for write to be safe
  int n = write(sock, raw, strlen(raw));
  if (n < 0) {
    logd("write go wrong, mostly beacuse the socket is closed");
    return E_WRITE_WRONG;
  }
  if (n < len) {
    logd("write incomplete");
    return E_WRITE_INCOMP;
  }
  return 1;
}

int writeBinary(int sock, char* raw, int len) {
  if (raw == NULL) return E_NULL;
  int n = write(sock, raw, len);
  if (n < 0) {
    logd("write go wrong, mostly beacuse the socket is closed");
    return E_WRITE_WRONG;
  }
  if (n < len) {
    logd("write incomplete");
    return E_WRITE_INCOMP;
  }
  return 1;
}

int writeFile(int sock, char* path) {
  char buffer[BUFFER_SIZE + 1];
  logd(path);
  // write it to socket
  // stop when accident happens
  // int fd = open(path, O_RDONLY);
  FILE* f = fopen(path, "rb");
  if (f == NULL) {
    loge(formatstr("cannot open the target file [%s]", path));
    return E_FILE_SYS;
  }

  int ret;
  int count = 0;
  // read from file trunk by trunk.
  while (1) {
    int n = fread(buffer, 1, BUFFER_SIZE, f);
    // TODO: using preferred I/O block size.
    if (n < 0) {
      loge(formatstr("cannot read the target file", path));
      return E_FILE_SYS;
    }
    if (n > 0) {
      count += n;
      ret = writeBinary(sock, buffer, n);
      if (ret < 0) {
        loge(formatstr("cannot write to the data socket %d", sock));
        return E_DSOCKET;
      }
    }
    if (n < BUFFER_SIZE) {
      /* it reach the file end, EOF will be sent later by shuting down */
      break;
    }
  }
  logi(formatstr("Finished to write file[%s] with %d bytes to socket %d", path,
                 count, sock));
  return 1;
}

int greeting(int sock) { return sendReply(sock, REPLY220); }

void clearConn(int ftp_socket, struct conn_info* info) {
  clearMode(info);
  shutdown(ftp_socket, SHUT_RDWR);
  // TODO: close needed here?
}

void runFTP(int ftp_socket, struct conn_info* info) {
  if (info == NULL) return;
  int ret = greeting(ftp_socket);
  if (ret < 0) {
    loge(formatstr("socket %d close unexpectely", ftp_socket));
    return;
  }

  struct sockaddr_in addr;
  socklen_t slen = sizeof(addr);
  getpeername(ftp_socket, (struct sockaddr*)&addr, &slen);
  info->peer_ip = addr.sin_addr.s_addr;
  info->id = -1;             /* not login at first */
  info->dserver_socket = -1; /* not data socket by now */
  info->d_socket = -1;
  info->mode = FTP_M_EMPTY;
  strcpy(info->work_dir,
         config.root); /* defautlt working dir is from the config */

  char result[BUFFER_SIZE];
  struct request req;
  while (1) {
    ret = readOneRequest(ftp_socket, result);
    if (ret < 0) {
      loge(formatstr("socket %d close unexpectely", ftp_socket));
      return;
    }
    ret = parseRawRequest(result, &req);
    if (ret < 0) {
      logw(formatstr("socket %d cannot understand this request: %s", ftp_socket,
                     result));
      sendReply(ftp_socket, REPLY500);
    } else {
      switch (req.type) {
          /* handle request */
        case FTP_USER: {
          if ((ret = handleLogin(ftp_socket, req, info)) == E_SOCKET_WRONG)
            return;
          break;
        }
        case FTP_PASS: {
          if ((ret = handleUnexpPass(ftp_socket, req, info)) == E_SOCKET_WRONG)
            return;
          break;
        }
        case FTP_PORT: {
          if ((ret = handlePort(ftp_socket, req, info)) == E_SOCKET_WRONG)
            return;
          break;
        }
        case FTP_PASV: {
          if ((ret = handlePasv(ftp_socket, req, info)) == E_SOCKET_WRONG)
            return;
          break;
        }
        case FTP_RETR: {
          if ((ret = handleRetr(ftp_socket, req, info)) == E_SOCKET_WRONG)
            return;
          break;
        }
        case FTP_TYPE: {
          if ((ret = handleType(ftp_socket, req, info)) == E_SOCKET_WRONG)
            return;
          break;
        }
        case FTP_SYST: {
          if ((ret = handleSyst(ftp_socket, req, info)) == E_SOCKET_WRONG)
            return;
          break;
        }
        case FTP_QUIT: {
          handleQuit(ftp_socket, req, info);
          return;
        }
        case FTP_ABOR: {
          handleAbor(ftp_socket, req, info);
          return;
        }
        case FTP_LIST: {
          if ((ret = handleList(ftp_socket, req, info)) == E_SOCKET_WRONG)
            return;
          break;
        }
        case FTP_MKD: {
          if ((ret = handleMkd(ftp_socket, req, info)) == E_SOCKET_WRONG)
            return;
          break;
        }
        case FTP_CWD: {
          if ((ret = handleCwd(ftp_socket, req, info)) == E_SOCKET_WRONG)
            return;
          break;
        }
        case FTP_PWD: {
          if ((ret = handlePwd(ftp_socket, req, info)) == E_SOCKET_WRONG)
            return;
          break;
        }
        case FTP_RMD: {
          if ((ret = handleRmd(ftp_socket, req, info)) == E_SOCKET_WRONG)
            return;
          break;
        }
        case FTP_RNFR: {
          if ((ret = handleRename(ftp_socket, req, info)) == E_SOCKET_WRONG)
            return;
          break;
        }
        case FTP_RNTO: {
          if ((ret = handleUnexpRnto(ftp_socket, req, info)) == E_SOCKET_WRONG)
            return;
          break;
        }
        default: {
          sendReply(ftp_socket, REPLY500);
        }
      }
    }
  }
}

int handleUnexpPass(int ftp_socket, struct request req,
                    struct conn_info* info) {
  clearMode(info);
  int ret = sendReply(ftp_socket, REPLY503);
  if (ret < 0) {
    loge(formatstr("socket %d close unexpectely", ftp_socket));
    return E_SOCKET_WRONG;
  }
  logi(formatstr("socket %d: recieve password [%s] unexpectly", ftp_socket,
                 req.params));
  return 1;
}

int handleUnexpRnto(int ftp_socket, struct request req,
                    struct conn_info* info) {
  clearMode(info);
  int ret = sendReply(ftp_socket, REPLY503);
  if (ret < 0) {
    loge(formatstr("socket %d close unexpectely", ftp_socket));
    return E_SOCKET_WRONG;
  }
  logi(formatstr("socket %d: recieve rnto [%s] unexpectly", ftp_socket,
                 req.params));
  return 1;
}

int handleLogin(int sock, struct request req, struct conn_info* info) {
  clearMode(info);
  if (info->id >= 0) {
    logw("try to log in repeatedly");
    /* if log success, change info->id. otherwise remains the same. */
  }

  int ret = sendReply(sock, REPLY331);
  if (ret < 0) {
    loge(formatstr("socket %d close unexpectely", sock));
    return E_SOCKET_WRONG;
  }
  char result[BUFFER_SIZE];

  struct request pass_req;
  ret = readOneRequest(sock, result);
  if (ret < 0) {
    return E_SOCKET_WRONG;
  }
  ret = parseRawRequest(result, &pass_req);

  if (ret < 0) {
    logw(formatstr("socket %d cannot understand this request: %s", sock,
                   result));
    ret = sendReply(sock, REPLY500);
    return 1;
  }

  if (pass_req.type != FTP_PASS) {
    loge("not understand this command sequences");
    ret = sendReply(sock, REPLY503);
    return 1;
  }

  logi(formatstr("socket %d: user [%s] login with password [%s]", sock,
                 req.params, pass_req.params));

  // TODO: user / pass word check.

  ret = sendReply(sock, REPLY230);

  if (ret < 0) {
    if (ret == E_SOCKET_WRONG) {
      loge(formatstr("socket %d close unexpectely", sock));
      return E_SOCKET_WRONG;
    }
  } else {
    info->id = 1;
  }
  return 1;
}

int handleRename(int ftp_socket, struct request req, struct conn_info* info) {
  clearMode(info);
  int ret;
  if ((ret = checkLogin(ftp_socket, info) < 0)) return ret;

  char* w = realpathForThread(info->work_dir, req.params);
  if (w == NULL || checkSub(info->work_dir, w)) {
    if (w == NULL)
      ret = sendReply(ftp_socket, REPLY550E);
    else
      ret = sendReply(ftp_socket, REPLY550P);
    if (ret < 0) {
      loge(formatstr("socket %d close unexpectely", ftp_socket));
      return E_SOCKET_WRONG;
    }
    return E_NOT_EXIST;
  } else
    ret = sendReply(ftp_socket, REPLY350);

  if (ret < 0) {
    loge(formatstr("socket %d close unexpectely", ftp_socket));
    return E_SOCKET_WRONG;
  }

  struct request rnto_req;
  char result[BUFFER_SIZE];
  ret = readOneRequest(ftp_socket, result);
  if (ret < 0) {
    return E_SOCKET_WRONG;
  }
  ret = parseRawRequest(result, &rnto_req);

  if (ret < 0) {
    logw(formatstr("socket %d cannot understand this request: %s", ftp_socket,
                   result));
    ret = sendReply(ftp_socket, REPLY500);
    return 1;
  }

  if (rnto_req.type != FTP_RNTO) {
    loge("not understand this command sequences");
    ret = sendReply(ftp_socket, REPLY503);
    return 1;
  }

  char* w_new = realpathForThread(info->work_dir, rnto_req.params);

  if (w_new != NULL) {
    struct reply r;
    char msg[BUFFER_SIZE];
    sprintf(msg, "File/Directory %.4096s already exists.", w_new);
    genReply(&r, 550, msg);
    ret = sendReply(ftp_socket, r);
  } else {
    logi(formatstr("socket %d: file [%s] change to  [%s]", ftp_socket,
                   req.params, rnto_req.params));
    logi(formatstr("%s->%s", w, rnto_req.params));

    pthread_mutex_lock(&dir_mutex);
    chdir(info->work_dir);

    // TODO: rnto_req.params permission check.

    ret = rename(w, rnto_req.params);

    chdir(config.root);
    pthread_mutex_unlock(&dir_mutex);

    if (ret < 0)
      ret = sendReply(ftp_socket, REPLY550);
    else
      ret = sendReply(ftp_socket, REPLY250);
  }

  if (ret == E_SOCKET_WRONG) {
    loge(formatstr("socket %d close unexpectely", ftp_socket));
    return E_SOCKET_WRONG;
  }
  return 1;
}

int clearMode(struct conn_info* info) {
  if (info->mode == FTP_M_EMPTY) return 1;
  if (info->mode == FTP_M_ERROR) {
    info->mode = FTP_M_EMPTY;
    return E_ALL;
  }
  if (info->mode == FTP_M_PASV_LISTEN) {
    info->mode = FTP_M_EMPTY;
    pthread_cancel(info->d_thread);
    shutdown(info->dserver_socket, SHUT_RDWR);
    close(info->dserver_socket);
    return 1;
  }
  if (info->mode == FTP_M_PASV_ACCEPT) {
    info->mode = FTP_M_EMPTY;
    pthread_cancel(info->d_thread);
    shutdown(info->d_socket, SHUT_RDWR);
    close(info->d_socket);
    return 1;
  }
  if (info->mode == FTP_M_PORT_WAIT) {
    info->mode = FTP_M_EMPTY;
    return 1;
  }
  return E_NOT_UNDERSTAND;
}

int handlePasv(int ftp_socket, struct request req, struct conn_info* info) {
  if (req.type != FTP_PASV) return E_NOT_UNDERSTAND;
  clearMode(info);

  int ret;
  if ((ret = checkLogin(ftp_socket, info) < 0)) return ret;

  int dserver_socket = newBindSocket(-1, config.ip);
  if (dserver_socket < 0) {
    loge("error when try to make a new socket");
    if ((ret = sendReply(ftp_socket, REPLY500)) < 0) {
      loge(formatstr("socket %d close unexpectely", ftp_socket));
      return E_SOCKET_WRONG;
    }
    return E_START_SOCKET;
  }

  struct sockaddr_in addr;
  socklen_t slen = sizeof(addr);
  ret = getsockname(dserver_socket, (struct sockaddr*)&addr, &slen);
  if (ret < 0) {
    loge("error when trying to reach the ip/port for data socket");
    if ((ret = sendReply(ftp_socket, REPLY500)) < 0) {
      loge(formatstr("socket %d close unexpectely", ftp_socket));
      shutdown(dserver_socket, SHUT_RDWR);
      return E_SOCKET_WRONG;
    }
    shutdown(dserver_socket, SHUT_RDWR);
    return E_DSOCKET;
  }

  int port = ntohs(addr.sin_port);
  logi(formatstr("data server socket %d is now in ip[%s] port[%d]",
                 dserver_socket, inet_ntoa(addr.sin_addr), port));

  if ((ret = listen(dserver_socket, config.max_connect)) < 0) {
    loge("cannot listen");
    shutdown(dserver_socket, SHUT_RDWR);
    return E_LISTEN;
  }
  logi(formatstr("listen ok... data server listening at port %d", port));

  /* update connection info: listen to dserver_socket */
  info->dserver_socket = dserver_socket;
  info->mode = FTP_M_PASV_LISTEN;

  ret = pthread_create(&(info->d_thread), NULL, syncData, info);
  if (ret < 0) {
    loge("cannot create new thread to listen");
    info->mode = FTP_M_EMPTY;
    shutdown(dserver_socket, SHUT_RDWR);
    return E_THREAD;
  }

  pthread_detach(ret);

  char result[BUFFER_SIZE];
  char* ip = inet_ntoa(addr.sin_addr);
  replaceChar(ip, '.', ',');
  struct reply r;
  ret = snprintf(result, BUFFER_SIZE, "=%s,%d,%d\r\n", ip, port / 256,
                 port % 256);

  if (ret < 0 || ret + 1 > BUFFER_SIZE) {
    logw("failed to gen a message send back");
    ret = sendReply(ftp_socket, REPLY500);
    if (ret < 0) {
      loge(formatstr("socket %d close unexpectely", ftp_socket));
      shutdown(dserver_socket, SHUT_RDWR);
      return E_SOCKET_WRONG;
    }
    shutdown(dserver_socket, SHUT_RDWR);
    return E_MSG_FAIL;
  }
  genReply(&r, 227, result);

  ret = sendReply(ftp_socket, r);
  if (ret < 0) {
    loge(formatstr("socket %d close unexpectely", ftp_socket));
    shutdown(dserver_socket, SHUT_RDWR);
    return E_START_SOCKET;
  }

  logi("PASV mode set ok");

  return 1;
}

int handlePort(int ftp_socket, struct request req, struct conn_info* info) {
  clearMode(info);

  int ret;
  if ((ret = checkLogin(ftp_socket, info) < 0)) return ret;

  ret = parseHostPort(req.params, &(info->pomode_ip), &(info->pomode_port));

  if (ret < 0) {
    loge(formatstr("socket %d: PORT parameter illegal.", ftp_socket));
    ret = sendReply(ftp_socket, REPLY500);
    if (ret < 0) {
      loge(formatstr("socket %d close unexpectely", ftp_socket));
      return E_START_SOCKET;
    }
    return E_MODE;
  }
  struct in_addr addr = {.s_addr = info->pomode_ip};
  logi(formatstr(
      "PORT mode: socket %d is now prepared to connect ip[%s] port[%d]",
      ftp_socket, inet_ntoa(addr), ntohs(info->pomode_port)));

  struct reply r;
  genReply(&r, 200, "PORT command successful\r\n");
  ret = sendReply(ftp_socket, r);

  if (ret < 0) {
    loge(formatstr("socket %d close unexpectely", ftp_socket));
    return E_START_SOCKET;
  }

  info->mode = FTP_M_PORT_WAIT;

  logi("PORT mode set ok");
  return 1;
}

int parseHostPort(char* raw, in_addr_t* host, in_port_t* port) {
  if (raw == NULL) return E_NULL;
  replaceChar(raw, ',', '.');
  int len = strlen(raw);
  int cnt = 0, point = 0;
  for (cnt = 0; cnt < len; cnt++) {
    if (raw[cnt] == '.') {
      point++;
      if (point == 4) {
        raw[cnt] = '\0';
        break;
      }
    }
  }
  if (point != 4) {
    return E_NOT_UNDERSTAND;
  }
  *host = inet_addr(raw + 1);

  if (*host == (in_addr_t)(-1)) {
    return E_NOT_UNDERSTAND;
  }
  int num1 = -1, num2 = -1;

  ++cnt;
  int st = cnt, ret;
  for (; cnt < len; cnt++) {
    if (raw[cnt] == '.') {
      raw[cnt] = '\0';
      if (!(ret = str2int(raw + st, &num1))) return E_NOT_UNDERSTAND;
      break;
    }
  }

  ++cnt;
  st = cnt;

  if (!(ret = str2int(raw + st, &num2))) return E_NOT_UNDERSTAND;

  if (num1 < 0 || num2 < 0) return E_NOT_UNDERSTAND;

  *port = htons(num1 * 256 + num2);

  return 1;
}

int handleRetr(int ftp_socket, struct request req, struct conn_info* info) {
  int ret;
  if ((ret = checkLogin(ftp_socket, info) < 0)) return ret;

  if (info->mode != FTP_M_PASV_ACCEPT && info->mode != FTP_M_PORT_WAIT) {
    logw("tcp connection not prepared");
    ret = sendReply(ftp_socket, REPLY425);
    if (ret < 0) {
      loge(formatstr("socket %d close unexpectely", ftp_socket));
      return E_SOCKET_WRONG;
    }
    return E_DSOCKET;
  }
  /* mode check ok, tcp connection is established */

  char* path = realpathForThread(info->work_dir, req.params);
  if (path == NULL) {
    logw(formatstr(
        "not exist : receive an unacceptable file path when RETR [%s]",
        req.params));
    if ((ret = sendReply(ftp_socket, REPLY550E)) < 0) {
      loge(formatstr("socket %d close unexpectely", ftp_socket));
      return E_SOCKET_WRONG;
    }
    info->mode = FTP_M_EMPTY;
    return E_NOT_EXIST;
  }

  if (!checkSub(path, config.root)) {
    logw(formatstr(
        "no access : receive an unacceptable file path when RETR [%s]",
        req.params));
    if ((ret = sendReply(ftp_socket, REPLY550P)) < 0) {
      loge(formatstr("socket %d close unexpectely", ftp_socket));
      return E_SOCKET_WRONG;
    }
    info->mode = FTP_M_EMPTY;
    return E_NO_ACCESS;
  }
  /* File check ok. Now trasfer it. */

  logd(formatstr("file & mode checked for RETR, we are about to transfer [%s]",
                 path));
  if ((ret = sendReply(ftp_socket, REPLY150)) < 0) {
    loge(formatstr("socket %d close unexpectely", ftp_socket));
    return E_SOCKET_WRONG;
  }

  if (info->mode == FTP_M_PORT_WAIT) {
    logd("port mode: try to prepare socket");
    ret = preparePortSocket(ftp_socket, info);
    if (ret < 0) {
      ret = sendReply(ftp_socket, REPLY425);
      if (ret < 0) {
        loge(formatstr("socket %d close unexpectely", ftp_socket));
        return E_SOCKET_WRONG;
      }
      return E_DSOCKET;
    }
    logd(formatstr("PORT mode: d_socket %d prepared", info->d_socket));
  }

  int d_socket = info->d_socket;

  // In current thread, transfer the file to d_socket
  ret = writeFile(d_socket, path);

  // everything is done, then send ftp_socket the reply
  if (ret != E_SOCKET_WRONG) {
    if (ret < 0) {
      if (ret == E_DSOCKET)
        ret = sendReply(ftp_socket, REPLY426);
      else
        ret = sendReply(ftp_socket, REPLY500);
    } else
      ret = sendReply(ftp_socket, REPLY226);
  }
  if (ret < 0) {
    loge(formatstr("socket %d close unexpectely", ftp_socket));
    return E_SOCKET_WRONG;
  }

  info->mode = FTP_M_EMPTY;
  shutdown(d_socket, SHUT_RDWR);

  return 1;
}

int handleList(int ftp_socket, struct request req, struct conn_info* info) {
  if (req.type != FTP_LIST) return E_NOT_UNDERSTAND;
  int ret;
  if ((ret = checkLogin(ftp_socket, info) < 0)) return ret;

  if (info->mode != FTP_M_PASV_ACCEPT && info->mode != FTP_M_PORT_WAIT) {
    logw("tcp connection not prepared");
    ret = sendReply(ftp_socket, REPLY425);
    if (ret < 0) {
      loge(formatstr("socket %d close unexpectely", ftp_socket));
      return E_SOCKET_WRONG;
    }
    return E_DSOCKET;
  }
  /* mode check ok, tcp connection is established */

  logd(formatstr("mode checked for RETR, we are about to LIST [%s]",
                 config.root));

  ret = checkWorkDir(info);
  if (ret < 0) {
    struct reply r;
    char msg[BUFFER_SIZE * 2];
    snprintf(msg, BUFFER_SIZE * 2,
             "Requested file action not taken.\r\nOriginal working dir go "
             "wrong.\r\nWe switched it to [%s].\r\n",
             info->work_dir);
    genReply(&r, 425, msg);
    if ((ret = sendReply(ftp_socket, r)) < 0) {
      return E_SOCKET_WRONG;
    }
    return E_WORK_DIR;
  }

  if ((ret = sendReply(ftp_socket, REPLY150D)) < 0) {
    loge(formatstr("socket %d close unexpectely", ftp_socket));
    return E_SOCKET_WRONG;
  }

  if (info->mode == FTP_M_PORT_WAIT) {
    logd("PORT mode: try to prepare socket");
    ret = preparePortSocket(ftp_socket, info);
    if (ret < 0) {
      ret = sendReply(ftp_socket, REPLY425);
      if (ret < 0) {
        loge(formatstr("socket %d close unexpectely", ftp_socket));
        return E_SOCKET_WRONG;
      }
      return E_DSOCKET;
    }
    logd(formatstr("PORT mode: d_socket %d prepared", info->d_socket));
  }

  int d_socket = info->d_socket;

  // In current thread, transfer the list msg to d_socket
  ret = writeListMessage(info);

  // everything is done, then send ftp_socket the reply
  if (ret != E_SOCKET_WRONG) {
    if (ret < 0)
      ret = sendReply(ftp_socket, REPLY500);
    else
      ret = sendReply(ftp_socket, REPLY226);
  }
  if (ret < 0) {
    loge(formatstr("socket %d close unexpectely", ftp_socket));
    return E_SOCKET_WRONG;
  }
  logi(formatstr("socket %d LIST transfer ok.", ftp_socket));

  info->mode = FTP_M_EMPTY;
  shutdown(d_socket, SHUT_RDWR);

  return 1;
}

int preparePortSocket(int ftp_socket, struct conn_info* info) {
  struct sockaddr_in addr;
  addr.sin_family = AF_INET;
  addr.sin_port = info->pomode_port;
  addr.sin_addr.s_addr = info->pomode_ip;
  int sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
  int ret;
  if (sock < 0) {
    ret = sendReply(ftp_socket, REPLY500);
    if (ret < 0) {
      loge(formatstr("socket %d close unexpectely", ftp_socket));
      return E_SOCKET_WRONG;
    }
    loge("failed to start socket");
    return E_START_SOCKET;
  }

  ret = connect(sock, (struct sockaddr*)&addr, sizeof(addr));
  if (ret < 0) {
    ret = sendReply(ftp_socket, REPLY500);
    if (ret < 0) {
      loge(formatstr("socket %d close unexpectely", ftp_socket));
      return E_SOCKET_WRONG;
    }
    loge("failed to connect socket");
    return E_START_SOCKET;
  }

  info->d_socket = sock;

  return 1;
}

int handleSyst(int ftp_socket, struct request req, struct conn_info* info) {
  if (req.type != FTP_SYST) return E_NOT_UNDERSTAND;
  clearMode(info);

  int ret;
  if ((ret = checkLogin(ftp_socket, info) < 0)) return ret;

  ret = sendReply(ftp_socket, REPLY215);
  if (ret < 0) {
    loge(formatstr("socket %d close unexpectely", ftp_socket));
    return E_SOCKET_WRONG;
  }
  logi(formatstr("socket %d: SYST received", ftp_socket));
  return 1;
}

int handleQuit(int ftp_socket, struct request req, struct conn_info* info) {
  int ret = sendReply(ftp_socket, REPLY221);
  if (ret < 0) {
    loge(formatstr("socket %d close unexpectely", ftp_socket));
    return E_SOCKET_WRONG;
  }
  logi(formatstr("socket %d: QUIT received with [%s]", ftp_socket, req.params));
  logi(formatstr("socket %d: current user %d", ftp_socket, info->id));
  return 1;
}

int handleAbor(int ftp_socket, struct request req, struct conn_info* info) {
  return handleQuit(ftp_socket, req, info);
}

int handleType(int ftp_socket, struct request req, struct conn_info* info) {
  int ret;
  if ((ret = checkLogin(ftp_socket, info) < 0)) return ret;

  if (req.params[0] == ' ') removeFirstSec(req.params, ' ');

  if (req.params[0] != 'I') {
    ret = sendReply(ftp_socket, REPLY500);
    if (ret < 0) {
      loge(formatstr("socket %d close unexpectely", ftp_socket));
      return E_SOCKET_WRONG;
    }
    return E_NOT_UNDERSTAND;
  }

  struct reply r;
  genReply(&r, 200, "Type set to I.\r\n");

  ret = sendReply(ftp_socket, r);
  if (ret < 0) {
    loge(formatstr("socket %d close unexpectely", ftp_socket));
    return E_SOCKET_WRONG;
  }
  logi(formatstr("socket %d: type set to %s", ftp_socket, req.params));
  return 1;
}

int handleMkd(int ftp_socket, struct request req, struct conn_info* info) {
  if (req.type != FTP_MKD) return E_NOT_UNDERSTAND;
  clearMode(info);

  int ret;
  if ((ret = checkLogin(ftp_socket, info) < 0)) return ret;

  ret = checkWorkDirAndReply(ftp_socket, info);
  if (ret < 0) return ret;

  int len = strlen(req.params);

  if (len == 0 || len > 255 ||
      realpathForThread(info->work_dir, req.params) != NULL) {
    loge("meet invalid parameter when create file");
    if (len == 0 || len > 255)
      ret = sendReply(ftp_socket, REPLY501);
    else {
      struct reply r;
      char msg[BUFFER_SIZE];
      snprintf(msg, BUFFER_SIZE,
               "Requested action not taken.\r\n[%.255s] already exists\r\n",
               req.params);
      logd(msg);
      genReply(&r, 550, msg);
      ret = sendReply(ftp_socket, r);
    }
    if (ret < 0) {
      loge(formatstr("socket %d close unexpectely", ftp_socket));
      return E_SOCKET_WRONG;
    }
    return E_NOT_UNDERSTAND;
  }

  pthread_mutex_lock(&dir_mutex);
  chdir(info->work_dir);
  ret = mkdir(req.params, S_IRUSR | S_IWUSR | S_IXUSR | S_IRGRP | S_IXGRP |
                              S_IROTH | S_IXOTH);
  chdir(config.root);
  pthread_mutex_unlock(&dir_mutex);

  if (ret < 0)
    ret = sendReply(ftp_socket, REPLY550);
  else {
    logi("dir make ok");
    struct reply r;
    char msg[BUFFER_SIZE + 20];
    sprintf(msg, "\"%s\" created\r\n", req.params);
    genReply(&r, 257, msg);
    ret = sendReply(ftp_socket, r);
  }
  if (ret < 0) {
    loge(formatstr("socket %d close unexpectely", ftp_socket));
    return E_SOCKET_WRONG;
  }
  return 1;
}

int handleCwd(int ftp_socket, struct request req, struct conn_info* info) {
  if (req.type != FTP_CWD) return E_NOT_UNDERSTAND;
  clearMode(info);

  int ret;
  if ((ret = checkLogin(ftp_socket, info) < 0)) return ret;

  ret = checkWorkDirAndReply(ftp_socket, info);
  if (ret < 0) return ret;

  char* w = realpathForThread(info->work_dir, req.params);

  if (checkDirectory(w) <= 0) {
    loge("meet invalid parameter when CWD");
    struct reply r;
    char msg[BUFFER_SIZE];
    snprintf(msg, BUFFER_SIZE,
             "Requested action not taken.\r\n[%.255s] not a directory\r\n",
             req.params);
    logd(msg);
    genReply(&r, 550, msg);
    ret = sendReply(ftp_socket, r);
    if (ret < 0) {
      loge(formatstr("socket %d close unexpectely", ftp_socket));
      return E_SOCKET_WRONG;
    }
    return E_NOT_UNDERSTAND;
  }

  if (!checkSub(w, config.root)) {
    ret = sendReply(ftp_socket, REPLY550P);
    if (ret < 0) {
      loge(formatstr("socket %d close unexpectely", ftp_socket));
      return E_SOCKET_WRONG;
    }
    return E_NO_ACCESS;
  }

  strcpy(info->work_dir, w);

  if (ret < 0)
    ret = sendReply(ftp_socket, REPLY550);
  else {
    logi(formatstr("socket %d working dir change to [%s]", ftp_socket,
                   info->work_dir));
    ret = sendReply(ftp_socket, REPLY250);
  }
  if (ret < 0) {
    loge(formatstr("socket %d close unexpectely", ftp_socket));
    return E_SOCKET_WRONG;
  }
  return 1;
}

int handleRmd(int ftp_socket, struct request req, struct conn_info* info) {
  if (req.type != FTP_RMD) return E_NOT_UNDERSTAND;
  clearMode(info);

  int ret;
  if ((ret = checkLogin(ftp_socket, info) < 0)) return ret;

  ret = checkWorkDirAndReply(ftp_socket, info);
  if (ret < 0) return ret;

  char* w = realpathForThread(info->work_dir, req.params);

  if (checkDirectory(w) <= 0) {
    loge("meet invalid parameter when create file");
    struct reply r;
    char msg[BUFFER_SIZE];
    snprintf(msg, BUFFER_SIZE,
             "Requested action not taken.\r\n[%.255s] not a directory\r\n",
             req.params);
    logd(msg);
    genReply(&r, 550, msg);
    ret = sendReply(ftp_socket, r);
    if (ret < 0) {
      loge(formatstr("socket %d close unexpectely", ftp_socket));
      return E_SOCKET_WRONG;
    }
    return E_NOT_UNDERSTAND;
  }

  logi(formatstr("%s %s", info->work_dir, config.root));

  if (checkSub(info->work_dir, w) || !checkSub(w, config.root)) {
    // 不能删除working dir以上的, 也不能删除config.root之外的
    ret = sendReply(ftp_socket, REPLY550P);
    if (ret < 0) {
      loge(formatstr("socket %d close unexpectely", ftp_socket));
      return E_SOCKET_WRONG;
    }
    return E_NO_ACCESS;
  }

  ret = rmdir(w);
  logi(formatstr("try to remove [%s]", w));

  if (ret < 0) {
    if (errno == EEXIST || errno == ENOTEMPTY)
      ret = sendReply(ftp_socket, REPLY550D);
    else
      ret = sendReply(ftp_socket, REPLY550);
  } else {
    logi(formatstr("socket %d dir removed", ftp_socket));
    ret = sendReply(ftp_socket, REPLY250);
  }
  if (ret < 0) {
    loge(formatstr("socket %d close unexpectely", ftp_socket));
    return E_SOCKET_WRONG;
  }
  return 1;
}

int handlePwd(int ftp_socket, struct request req, struct conn_info* info) {
  if (req.type != FTP_PWD) return E_NOT_UNDERSTAND;
  clearMode(info);

  int ret;
  if ((ret = checkLogin(ftp_socket, info) < 0)) return ret;

  ret = checkWorkDirAndReply(ftp_socket, info);
  if (ret < 0) return ret;

  if (ret < 0)
    ret = sendReply(ftp_socket, REPLY550);
  else {
    struct reply r;
    char msg[BUFFER_SIZE];
    snprintf(msg, BUFFER_SIZE, "%.4096s\r\n", info->work_dir);
    genReply(&r, 257, msg);
    ret = sendReply(ftp_socket, r);
    logi(formatstr("socket %d pwd ok", ftp_socket));
  }
  if (ret < 0) {
    loge(formatstr("socket %d close unexpectely", ftp_socket));
    return E_SOCKET_WRONG;
  }
  return 1;
}

int checkLogin(int ftp_socket, struct conn_info* info) {
  int ret;
  if (info->id < 0) {
    ret = sendReply(ftp_socket, REPLY530);
    if (ret < 0) {
      loge(formatstr("socket %d close unexpectely", ftp_socket));
      return E_SOCKET_WRONG;
    }
    return E_NO_ACCESS;
  }
  return 1;
}

int checkWorkDir(struct conn_info* info) {
  if (checkDirectory(info->work_dir) <= 0) {
    logw(formatstr("working dir check fail for [%s], switch to [%s]",
                   info->work_dir, config.root));
    strncpy(info->work_dir, config.root, BUFFER_SIZE);
    return E_NOT_EXIST;
  }
  return 1;
}

int checkWorkDirAndReply(int ftp_socket, struct conn_info* info) {
  int ret;
  if (checkDirectory(info->work_dir) <= 0) {
    logw(formatstr("working dir check fail for [%s], switch to [%s]",
                   info->work_dir, config.root));
    strncpy(info->work_dir, config.root, BUFFER_SIZE);
    struct reply r;
    char msg[BUFFER_SIZE * 2];
    snprintf(msg, BUFFER_SIZE * 2,
             "Requested file action not taken.\r\nOriginal working dir go "
             "wrong.\r\nWe switched it to [%s].\r\n",
             info->work_dir);
    genReply(&r, 425, msg);
    if ((ret = sendReply(ftp_socket, r)) < 0) {
      loge(formatstr("socket %d close unexpectely", ftp_socket));
      return E_SOCKET_WRONG;
    }
    return E_WORK_DIR;
  }
  return 1;
}

int writeListMessage(struct conn_info* info) {
  if (info == NULL) return E_NULL;

  DIR* dir;
  struct dirent* sub;
  struct stat buffer;
  dir = opendir(info->work_dir);
  unsigned int maxlink = 1, user_len = 1, group_len = 1;
  if (dir == NULL) return E_NOT_DIR;

  while ((sub = readdir(dir)) != NULL) {
    if (strcmp(sub->d_name, ".") == 0 || strcmp(sub->d_name, "..") == 0)
      continue;
    char* path = realpathForThread(info->work_dir, sub->d_name);
    if (path == NULL) continue;

    stat(path, &buffer);

    struct passwd* pwd = getpwuid(buffer.st_uid);
    struct group* gp = getgrgid(buffer.st_gid);
    if (buffer.st_nlink > maxlink) maxlink = buffer.st_nlink;
    if (strlen(pwd->pw_name) > user_len) user_len = strlen(pwd->pw_name);
    if (strlen(gp->gr_name) > group_len) group_len = strlen(pwd->pw_name);
  }

  int linkd = (int)(log10(maxlink) + 1);
  int d_socket = info->d_socket;

  dir = opendir(info->work_dir);
  if (dir == NULL) return E_NOT_DIR;
  while ((sub = readdir(dir)) != NULL) {
    if (strcmp(sub->d_name, ".") == 0 || strcmp(sub->d_name, "..") == 0)
      continue;
    char* path = realpathForThread(info->work_dir, sub->d_name);
    if (path == NULL) continue;

    stat(path, &buffer);

    char type = '-';
    if (S_ISBLK(buffer.st_mode)) type = 'b';
    if (S_ISCHR(buffer.st_mode)) type = 'c';
    if (S_ISDIR(buffer.st_mode)) type = 'd';
    if (S_ISLNK(buffer.st_mode)) type = 'l';
    if (S_ISFIFO(buffer.st_mode)) type = 'p';
    if (S_ISSOCK(buffer.st_mode)) type = 's';

    char permission[10] = "---------";

    if (S_IRUSR & buffer.st_mode) permission[0] = 'r';
    if (S_IWUSR & buffer.st_mode) permission[1] = 'w';
    if (S_IXUSR & buffer.st_mode) permission[2] = 'x';
    if (S_IRGRP & buffer.st_mode) permission[3] = 'r';
    if (S_IWGRP & buffer.st_mode) permission[4] = 'w';
    if (S_IXGRP & buffer.st_mode) permission[5] = 'x';
    if (S_IROTH & buffer.st_mode) permission[6] = 'r';
    if (S_IWOTH & buffer.st_mode) permission[7] = 'w';
    if (S_IXOTH & buffer.st_mode) permission[8] = 'x';

    // TODO: s / S

    struct passwd* pwd = getpwuid(buffer.st_uid);
    struct group* gp = getgrgid(buffer.st_gid);
    char msg[BUFFER_SIZE + 1];
    snprintf(msg, BUFFER_SIZE, "%c%s %*d %-*s %-*s %13d %.12s %.4s %s\r\n",
             type, permission, linkd, (int)buffer.st_nlink, user_len,
             pwd->pw_name, group_len, gp->gr_name, (int)buffer.st_size,
             ctime(&buffer.st_mtime) + 4, ctime(&buffer.st_mtime) + 20,
             sub->d_name);
    writeRaw(d_socket, msg);
  }
  closedir(dir);
  return 1;
}

char* realpathForThread(char* workdir, char* path) {
  if (workdir == NULL || path == NULL) return NULL;
  pthread_mutex_lock(&dir_mutex);
  char* r;
  int ret = chdir(workdir);
  if (ret < 0)
    r = NULL; /* change dir to workdir failed */
  else {
    r = realpath(path, NULL);
    chdir(config.root);
  }
  pthread_mutex_unlock(&dir_mutex);
  return r;
}

void* syncRun(void* socket_ptr) {
  int ftp_socket = (int)(*((int*)socket_ptr));
  logi(formatstr("ftp_socket accept. This is now a ftp_socket %d", ftp_socket));
  struct conn_info info;
  runFTP(ftp_socket, &info);
  clearConn(ftp_socket, &info);
  int id = socket2thread(ftp_socket);
  sk2th[id] = -1;
  logi(formatstr("ftp_socket %d finish and release threads pool %d", ftp_socket,
                 id));
  return NULL;
}

void* syncData(void* info_ptr) {
  struct conn_info* info = (struct conn_info*)info_ptr;
  /* waiting someone connect the data port */
  while (1) {
    struct sockaddr_in addr;
    socklen_t s_len = sizeof(addr);
    int new_socket =
        accept(info->dserver_socket, (struct sockaddr*)&addr, &s_len);
    logi("some one knock the data server door, we try to accept it");
    if (info->peer_ip != addr.sin_addr.s_addr) { /* avoid attackers */
      loge(formatstr("we refuse a unknown ip's [%s] connect.\n",
                     inet_ntoa(addr.sin_addr)));
      continue;
    }
    if (new_socket < 0) {
      loge("failed to accept connection");
      continue;
    }
    //    if( addr.sin_addr.s_addr !=
    logi("accept ok");
    info->d_socket = new_socket;
    break;
  }
  shutdown(info->dserver_socket, SHUT_RDWR);  // close the door

  info->dserver_socket = -1;
  info->mode = FTP_M_PASV_ACCEPT;
  logi(formatstr("data connection established. Accept in socket %d",
                 info->d_socket));
  return NULL;
}

void runSocket(int sock) {
  int id = getNewThreadId();
  if (id < 0) {
    loge("no more space for new thread");
    return;
  }
  sk2th[id] = sock;
  int ret = pthread_create(&threads[id], NULL, syncRun, &sk2th[id]);
  if (ret < 0) {
    loge("cannot create new thread");
    sk2th[id] = -1;
    return;
  }
  pthread_detach(threads[id]);
}