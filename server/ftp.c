#include "ftp.h"

pthread_t threads[MAX_CONN];
int sk2th[MAX_CONN];
char socket_buffer[BUFFER_SIZE];

void startServer(int argc, char* argv[]) {
  logi("[WELCOME]welcome to server");
  if (parseArgument(argc, argv) <= 0 || checkConfig() <= 0) {
    bye();
    return;
  }
  int ret = startListen(config.port, config.max_connect);
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
    int new_socket = accept(server_socket, NULL, NULL);  // TODO
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

int socket2thread(int socket) {
  for (int i = 0; i < MAX_CONN; i++) {
    if (sk2th[i] == socket) return i;
  }
  return -1;
}

int readOneRequest(int socket, char* result) {
  int count = 0, former_r = 0;
  while (1) {
    int n = read(socket, result + count, 1);
    if (n == 0) {
      logd("read the EOF end but no \\r\\n here");
      return E_READ_EOF;
    }
    if (n < 0) {
      logd("read go wrong, mostly beacuse the socket is closed");
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
      former_r = 1;
    count += n;

    /* need to read next char, but there is no space for '\0'(additonal +1) */
    if (count + 1 >= BUFFER_SIZE) {
      return E_OVER_BUFFER;
    }
  }
}

int sendReply(int socket, struct reply r) {
  int count = 0, former_r = 0;
  char raw[BUFFER_SIZE * 4];

  /* gen reply raw */
  int newline = 1, lastd = -1, cnt = 0;
  for (int i = 0; i < strlen(r.msg); i++) {
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

  int n = write(socket, raw, strlen(raw));
  if (n < 0) {
    logd("write go wrong, mostly beacuse the socket is closed");
    return E_WRITE_WRONG;
  }
  if (n < cnt) {
    logd("write");
    return E_WRITE_INCOMP;
  }

  return 1;
}

void processRaw(char* raw) {
  if (raw == NULL) return;
  int len = strlen(raw);
  if (raw[len - 1] == '\n' && raw[len - 2] == '\r') {
    raw[len - 2] = '\0';
    len -= 2;
  }
  if (raw[0] == ' ') removeFirstSec(raw, ' ');
  removeFirstSec(raw, ' ');
  logd(formatstr("raw information: [%d]", raw));
}

int parseRawRequest(char* raw, struct request* req) {
  if (req == NULL || raw == NULL) return E_NULL;
  processRaw(raw);
  /* 4-letter */
  if (startswith(raw, "USER")) {
    req->type = FTP_USER;
    strcpy(req->params, raw + 4);
    return 1;
  } else if (startswith(raw, "PASS")) {
    req->type = FTP_PASS;
    strcpy(req->params, raw + 4);
    return 1;
  } else if (startswith(raw, "PASV")) {
    req->type = FTP_PASV;
    strcpy(req->params, raw + 4);
    return 1;
  }
  return E_NOT_UNDERSTAND;
}

int greeting(int socket) { return sendReply(socket, REPLY220); }

void clearConn(struct conn_info* info) {
  // TODO: gracefully maybe?
  pthread_cancel(info->d_thread);
  if (info->d_socket > 0) shutdown(info->d_socket, SHUT_RDWR);
  if (info->dserver_socket > 0) shutdown(info->dserver_socket, SHUT_RDWR);
  info->mode = FTP_M_EMPTY;
}

void runFTP(int socket, struct conn_info* info) {
  if (info == NULL) return;
  int ret = greeting(socket);
  if (ret < 0) {
    loge(formatstr("socket %d close unexpectely", socket));
    return;
  }

  info->id = -1;             /* not login at first */
  info->dserver_socket = -1; /* not data socket by now */
  info->d_socket = -1;
  info->mode = FTP_ERROR;

  char result[BUFFER_SIZE];
  struct request req;
  while (1) {
    int ret = readOneRequest(socket, result);
    if (ret < 0) {
      loge(formatstr("socket %d close unexpectely", socket));
      return;
    }
    logd(formatstr("socket %d read a new request: %s", socket, result));
    ret = parseRawRequest(result, &req);
    if (ret < 0) {
      logw(formatstr("socket %d cannot understand this request: %s", socket,
                     result));
      sendReply(socket, REPLY500);
    } else {
      switch (req.type) {
          /* handle request */
        case FTP_USER: {
          if ((ret = handleLogin(socket, req, info)) == E_SOCKET_WRONG) return;
          break;
        }
        case FTP_PASS: {
          if ((ret = handleUnexpPass(socket, req, info)) == E_SOCKET_WRONG)
            return;
          break;
        }
        case FTP_PASV: {
          if ((ret = handlePasv(socket, req, info)) == E_SOCKET_WRONG) return;
          break;
        }
        default: {
          sendReply(socket, REPLY500);
        }
      }
    }
  }
}

int handleUnexpPass(int socket, struct request req, struct conn_info* info) {
  int ret = sendReply(socket, REPLY503);
  if (ret < 0) {
    loge(formatstr("socket %d close unexpectely", socket));
    return E_SOCKET_WRONG;
  }
  logi(formatstr("socket %d: recieve password [%s] unexpectly", socket,
                 req.params));
  return 1;
}

int handleLogin(int socket, struct request req, struct conn_info* info) {
  if (info->id >= 0) {
    logw("try to log in repeatedly");
    // TODO: what happened here?
  }

  int ret = sendReply(socket, REPLY331);
  if (ret < 0) {
    loge(formatstr("socket %d close unexpectely", socket));
    return E_SOCKET_WRONG;
  }
  char result[BUFFER_SIZE];

  struct request pass_req;
  ret = readOneRequest(socket, result);
  if (ret < 0) {
    return E_SOCKET_WRONG;
  }
  logd(formatstr("socket %d read a new request: %s", socket, result));

  ret = parseRawRequest(result, &pass_req);

  if (ret < 0) {
    logw(formatstr("socket %d cannot understand this request: %s", socket,
                   result));
    ret = sendReply(socket, REPLY500);
    return 1;
  }

  if (pass_req.type != FTP_PASS) {
    loge("not understand this command sequences");
    ret = sendReply(socket, REPLY503);
    return 1;
  }

  logi(formatstr("socket %d: user [%s] login with password [%s]", socket,
                 req.params, pass_req.params));

  // TODO: user / pass word check.

  ret = sendReply(socket, REPLY230);

  if (ret < 0) {
    if (ret == E_SOCKET_WRONG) {
      loge(formatstr("socket %d close unexpectely", socket));
      return E_SOCKET_WRONG;
    }
  } else {
    info->id = ret;
  }
  return 1;
}

int handlePasv(int ftp_socket, struct request req, struct conn_info* info) {
  // TODO: check if there is already in some mode.
  // TODO: clear the mode information and close the data thread.

  int ret;
  int dserver_socket = newBindSocket(-1, config.ip);
  if (dserver_socket < 0) {
    loge("error when try to make a new socket");
    if ((ret = sendReply(ftp_socket, REPLY500)) < 0) {
      loge(formatstr("socket %d close unexpectely", socket));
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
      loge(formatstr("socket %d close unexpectely", socket));
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
  struct reply r;
  ret = snprintf(result, BUFFER_SIZE, "=%s.%d.%d", inet_ntoa(addr.sin_addr),
                 port / 256, port % 256);

  if (ret < 0 || ret + 1 > BUFFER_SIZE) {
    logw("failed to gen a message send back");
    ret = sendReply(ftp_socket, REPLY500);
    if (ret < 0) {
      loge(formatstr("socket %d close unexpectely", socket));
      shutdown(dserver_socket, SHUT_RDWR);
      return E_SOCKET_WRONG;
    }
    shutdown(dserver_socket, SHUT_RDWR);
    return E_MSG_FAIL;
  }
  genReply(&r, 227, result);

  ret = sendReply(ftp_socket, r);
  if (ret < 0) {
    loge(formatstr("socket %d close unexpectely", socket));
    shutdown(dserver_socket, SHUT_RDWR);
    return E_START_SOCKET;
  }

  logi("PASV mode ok");

  return 1;
}

void* syncRun(void* socket_ptr) {
  int socket = (int)(*((int*)socket_ptr));
  logi(formatstr("socket accept. This is now a socket %d", socket));
  struct conn_info info;
  runFTP(socket, &info);
  clearConn(&info);
  shutdown(socket, SHUT_RDWR);
  int id = socket2thread(socket);
  sk2th[id] = -1;
  logi(formatstr("socket %d finish and release threads pool %d", socket, id));
  return NULL;
}

void* syncData(void* info_ptr) {
  struct conn_info* info = (struct conn_info*)info_ptr;
  /* waiting someone connect the data port */
  while (1) {
    int new_socket = accept(info->dserver_socket, NULL,
                            NULL);  // TODO: check ip address to avoid attacker
    intd(new_socket);
    logi("some one knock the data server door, we try to accept it");
    if (new_socket < 0) {
      loge("failed to accept connection");
      continue;
    }
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

void runSocket(int socket) {
  // TODO: try / catch
  int id = getNewThreadId();
  if (id < 0) {
    loge("no more space for new thread");
    return;
  }
  sk2th[id] = socket;
  int ret = pthread_create(&threads[id], NULL, syncRun, &sk2th[id]);
  if (ret < 0) {
    loge("cannot create new thread");
    sk2th[id] = -1;
    return;
  }
  pthread_detach(threads[id]);
}