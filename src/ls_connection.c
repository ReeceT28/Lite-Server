#include "ls_connection.h"
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
/* Function to create a listening socket from config */
int ls_create_lstning_sock(ls_lstning_sock_t* sock)
{

    printf("Creating socket: family=%d, type=%d\n", sock->config.family, sock->config.socktype);

    /* Create a socket */
    int fd = socket(sock->config.family, sock->config.socktype, 0);
    if (fd == -1) {
        perror("Listening Socket");
        return -1;
    }

    int opt = 1;

    // Always set SO_REUSEADDR
    if (setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
      perror("setsockopt SO_REUSEADDR");
      close(fd);
      return -1;
    }

#ifdef SO_REUSEPORT
  if (setsockopt(fd, SOL_SOCKET, SO_REUSEPORT, &opt, sizeof(opt)) < 0) {
    perror("setsockopt SO_REUSEPORT");
    close(fd);
    return -1;
  }
#endif

  // Make socket non-blocking
  int flags = fcntl(fd, F_GETFL, 0);
  if (flags < 0)
    flags = 0;
  if (fcntl(fd, F_SETFL, flags | O_NONBLOCK) < 0) {
    perror("Non-Blocking Listening Socket");
    close(fd);
    return -1;
  }

  if (sock->config.family == AF_INET) {
    struct sockaddr_in *addr = (struct sockaddr_in *)&sock->sockaddr;
    memset(addr, 0, sizeof(*addr));

    addr->sin_family = AF_INET;
    addr->sin_port = htons(sock->config.port);
    addr->sin_addr.s_addr =
        sock->config.host ? inet_addr(sock->config.host) : htonl(INADDR_ANY);

    if (bind(fd, (struct sockaddr *)addr, sizeof(*addr)) < 0) {
      perror("Bind Listening Socket");
      close(fd);
      return -1;
    }

    sock->socklen = sizeof(*addr);
  } else {
    printf("Only AF_INET is implemented for now\n");
    close(fd);
    return -1;
  }

  // Listen
  if (listen(fd, sock->config.backlog) < 0) {
    perror("listen");
    close(fd);
    return -1;
  }

  sock->fd = fd;
  printf("Listening on port %d\n", sock->config.port);
  return 0;
}
