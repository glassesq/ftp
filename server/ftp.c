#include "ftp.h"

pthread_t threads[MAX_CONN];
int sk2th[MAX_CONN];

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

void* syncRun(void* socket_ptr) {
  int socket = (int)(*((int*)socket_ptr));
  logi(formatstr("socket accept. This is now a socket %d", socket));
  // TODO
  // char str[] = "abcdef";
  // while (1) {
  //  write(socket, str, 3);
  //  logd(formatstr("socket %d write something.", socket));
  //  sleep(1);
  //}

  int id = socket2thread(socket);
  sk2th[id] = -1;
  logi(formatstr("socket %d finish and released with pos %d", socket, id));
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