#include "ls_connection.h"
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>

ls_lstning_sock_t create_lstning_sock()
{
    ls_lstning_sock_t lstning_sock;
    struct sockaddr server;
    // Create listening socket, just use these options for now for simplicity,
    // will expand more later
    int lstning_sock_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (lstning_sock_fd == -1) {
        printf("%s\n", strerror(errno));
    }
    // Make the socket have non blocking  IO
    fcntl(lstning_sock_fd, O_NONBLOCK);

    lstning_sock.fd = lstning_sock_fd;

    return lstning_sock;
}
