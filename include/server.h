#pragma once
#include <stdlib.h>
#include <stdint.h>
#include "server_config.h"
#include "http_parser.h"



#define MAX_CLIENT_BUFFER 8192

typedef struct http_client_s
{
    int fd;                         /* Socket fd */
    char buffer[MAX_CLIENT_BUFFER]; /* Receive buffer */
    size_t buffer_len;              /* Bytes currently in buffer */
    int request_in_progress;        /* Partial request flag */

    http_request req;               /* Associated request */
} http_client_t;

#define MAX_CLIENTS 1024

typedef struct
{
    lite_server_config config;
    http_client_t clients[MAX_CLIENTS];
    size_t client_count;
} lite_server;
