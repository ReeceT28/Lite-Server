#define _GNU_SOURCE
#include <stdio.h>
#include <string.h>
#include <sys/epoll.h>
#include <unistd.h>
#include <fcntl.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include "ls_logging.h"
#include "ls_connection.h"

/* Function to create a listening socket from config */
int ls_create_lstning_sock(ls_lstning_sock_t* sock)
{
    ls_log_cfg_t* log_cfg = sock->worker->server->log_cfg;
    char foo[69];
    snprintf(foo, sizeof(foo), "DEBUG: Creating listening socket on port %d\n", sock->config.port);
    ls_log_write(log_cfg, foo, strlen(foo), LS_LOG_DEBUG);

    /* Create a socket */
    int fd = socket(sock->config.family, sock->config.socktype, 0);
    if (fd == -1) {
        const char* msg = "WARNING: Failed to create a listening socket\n";
        ls_log_write(log_cfg, msg, strlen(msg), LS_LOG_WARNING);
        return -1;
    }

    int opt = 1;


    /* Set SO_REUSEADDR so that restarting the server is much faster, however apparently this can carry some risk, reference:
     * https://stackoverflow.com/questions/3229860/what-is-the-meaning-of-so-reuseaddr-setsockopt-option-linux#3233022 
     */
    if (setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) == -1) {
        const char* msg = "WARNING: Failed to set SO_REUSEADDR to true\n";
        ls_log_write(log_cfg, msg, strlen(msg), LS_LOG_WARNING);
        close(fd);
        return -1;
    }

    /* SO_REUSEPORT doesn't exist on all platforms but it is used to handle load balancing between workers of a server, exists in linux kernel >= 3.9 */
    if (setsockopt(fd, SOL_SOCKET, SO_REUSEPORT, &opt, sizeof(opt)) == -1) {
        const char* msg = "WARNING: Failed to set SO_REUSEPORT to true\n";
        ls_log_write(log_cfg, msg, strlen(msg), LS_LOG_WARNING);
        close(fd);
        return -1;
    }

    /* Get the current file status flags of the listening socket I believe this should just be 0 but might as well be safe for now */
    int flags = fcntl(fd, F_GETFL, 69);
    if (flags == -1) {
        const char* msg = "WARNING: Failed to get status flags of listening socket during creation\n";
        ls_log_write(log_cfg, msg, strlen(msg), LS_LOG_WARNING);
        close(fd);
        return -1;
    }
    /* Set the file status flags of the listening socket to make the sockets non-blocking */
    if (fcntl(fd, F_SETFL, flags | O_NONBLOCK) == -1) {
        close(fd);
        const char* msg = "WARNING: Failed to set listening socket to non blocking during creation\n";
        ls_log_write(log_cfg, msg, strlen(msg), LS_LOG_WARNING);
        return -1;
    }

    /* Configure socket for IPv4 or IPv6, only bothering with IPv4 currently, however, I have tried to make most parts of the program independent of IPv4 or IPv6 e.g. logging */
    if (sock->config.family == AF_INET) {
        /* setup addr to point to the sockaddr_storage* in the socket but cast to sockaddr_in* as we are using IPv4 */
        struct sockaddr_in *addr = (struct sockaddr_in *)&sock->sockaddr;
        /* Make sure no rubbish in memory */
        memset(addr, 0, sizeof(*addr));
        addr->sin_family = AF_INET;
        addr->sin_port = htons(sock->config.port);
        /* If host is NULL then bind to all available network interfaces otherwise bind to the specified one */
        addr->sin_addr.s_addr = sock->config.host ? inet_addr(sock->config.host) : htonl(INADDR_ANY);
        /* Assigns addr to the socket */
        if (bind(fd, (struct sockaddr*)addr, sizeof(*addr)) == -1) {
            const char* msg = "WARNING: Failed to bind listening socket during creation\n";
            ls_log_write(log_cfg, msg, strlen(msg), LS_LOG_WARNING);
            close(fd);
            return -1;
        }

        sock->socklen = sizeof(*addr);
    } else {
        const char* msg = "WARNING: IPv6 is not supported for listening sockets yet\n";
        ls_log_write(log_cfg, msg, strlen(msg), LS_LOG_WARNING);
        close(fd);
        return -1;
    }

    /* Start the socket listening */
    if (listen(fd, sock->config.backlog) == -1) {
        perror("listen");
        close(fd);
        return -1;
    }

    sock->fd = fd;
    printf("Listening on port %d\n", sock->config.port);
    return 0;
}

void ls_close_connection(ls_connection_t *conn)
{
    if (conn->closed) return;
    conn->closed = 1;
    epoll_ctl(conn->worker->epfd, EPOLL_CTL_DEL, conn->fd, NULL);
    close(conn->fd);
    if (conn->pool) {
      ls_free_pool(conn->pool);
      conn->pool = NULL;
    }
    conn->protocol_ctx = NULL;
}
