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

int startListen(int port, int connection) {
  init();

  /* create the socket */
  int server_socket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
  if (server_socket < 0) {
    loge("failed to start socket");
    return E_START_SOCKET;
  }
  logd(formatstr("server socket ok, it is %d.", server_socket));

  /* set server's address */
  struct sockaddr_in server_addr;
  memset(&server_addr, 0, sizeof(server_addr));
  server_addr.sin_family = AF_INET;
  server_addr.sin_port = htons(port);
  server_addr.sin_addr.s_addr = htonl(INADDR_ANY);  // inet_addr("127.0.0.1")
  logd("server addr generated");

  /* bind server to its address */
  int ret =
      bind(server_socket, (struct sockaddr*)&server_addr, sizeof(server_addr));
  if (ret < 0) {
    loge(formatstr("failed to bind to %d", port));
    return E_BIND;
  }
  logd("bind ok");

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
      logi("one new request read");
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

int parseRawRequest(char* raw, struct request* req) {
  if (req == NULL || raw == NULL) return E_NULL;
  removeChar(raw, ' ');
  removeChar(raw, '\r');
  removeChar(raw, '\n');
  /* 4-letter */
  if (strncmp("USER", raw, 4) == 0) {
    req->type = FTP_USER;
    strcpy(req->params, raw + 4);
    return 1;
  } else if (strncmp("PASS", raw, 4) == 0) {
    req->type = FTP_PASS;
    strcpy(req->params, raw + 4);
    return 1;
  }
  return E_NOT_UNDERSTAND;
}

int greeting(int socket) { return sendReply(socket, REPLY220); }

void runFTP(int socket) {
  int ret = greeting(socket);
  if (ret < 0) {
    loge(formatstr("socket %d close unexpectely", socket));
    return;
  }
  struct conn_info info;
  info.id = -1; /* not login at first */

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
          if ((ret = handleLogin(socket, req, &info)) < 0) return;
          break;
        }
        default: {
          sendReply(socket, REPLY500);
        }
      }
    }
  }
}

int handleLogin(int socket, struct request req, struct conn_info* info) {
  if (info->id >= 0) {
    logw("try to log in repeatedly");
    sendReply(socket, REPLY500);
    return 0;
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
    return E_NOT_UNDERSTAND;
  }

  if (pass_req.type != FTP_PASS) {
    loge("not understand this command sequences");
    ret = sendReply(socket, REPLY503);
    return E_NOT_UNDERSTAND;
  }

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

void* syncRun(void* socket_ptr) {
  int socket = (int)(*((int*)socket_ptr));
  logi(formatstr("socket accept. This is now a socket %d", socket));
  runFTP(socket);
  int id = socket2thread(socket);
  sk2th[id] = -1;
  logi(formatstr("socket %d finish and release threads pool %d", socket, id));
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