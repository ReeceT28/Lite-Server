#pragma once
#include <sys/socket.h>
#include "ls_mem_pool.h"
#include "ls_event.h"
#include "ls_server.h"

/* Configuration for listening sockets */
typedef struct ls_socket_conf_s {
    int family;
    int socktype;
    const char *host;
    uint16_t port;
    int backlog;
    int type;
} ls_socket_conf_t;

enum {
  LS_SOCK_HTTP,
  /* Add more protocols here later */
};
/* Represents a listening socket, essentially our server */
typedef struct ls_lstning_sock_s {
    int fd;
    struct sockaddr_storage sockaddr;
    socklen_t socklen;
    ls_worker_t* worker;
    ls_socket_conf_t config;
}   ls_lstning_sock_t;

/* Represents a connection (a client) */
typedef struct ls_connection_s {
    int fd;
    ls_event_t read_event;
    ls_event_t write_event;
    ls_mem_pool_t* pool;
    ls_worker_t* worker;
    void* protocol_ctx;
    uint64_t expire_at;
    size_t index;
    int closed;
} ls_connection_t;



int ls_create_lstning_sock(ls_lstning_sock_t* sock);
void ls_close_connection(ls_connection_t* conn);