#pragma once
#include "ls_event.h"
#include <sys/socket.h>

/* Represents a listening socket, essentially our server */
typedef struct ls_sock_lstn_s { 
    int fd;
    struct sockaddr *sockaddr; 
    socklen_t socklen; 
} ls_server_t;

/* Represents a connection (a client) */
typedef struct ls_connection_s {
    int fd;
    ls_event_t* read_event;
    ls_event_t* write_event; 
} ls_connection_t;