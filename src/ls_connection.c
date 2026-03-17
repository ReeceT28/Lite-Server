#include "ls_connection.h"
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>

int ls_create_lstning_sock(ls_lstning_sock_t* lstning_sock)
{
    struct sockaddr server;
    // Create listening socket, just use these options for now for simplicity, will expand more later
    int lstning_sock_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (lstning_sock_fd == -1) {
        printf("%s\n", strerror(errno));
        return -1;
    }
    // Make the socket have non blocking  IO
    if( fcntl(lstning_sock_fd, F_SETFL, O_NONBLOCK) == -1) {
        printf("%s\n", strerror(errno));
        return -1;
    }

    lstning_sock->fd = lstning_sock_fd;

    return 0;
}
