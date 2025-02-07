#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <unistd.h>
#include <errno.h>

#define MAX_SOCKETS 10
#define PKT_LEN 1400

int sans_connect(const char* host, int port, int protocol) {
  return 0;
}

int sans_accept(const char* addr, int port) {
  return 0;
}

int sans_disconnect(int socket) {
  return 0;
}
