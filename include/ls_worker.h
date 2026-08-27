#pragma once
#include "ls_connection.h"
#include "ls_mem_pool.h"

typedef struct ls_log_cfg_s ls_log_cfg_t;
typedef struct ls_connection_s ls_connection_t;
typedef struct ls_server_context_s ls_server_context_t;

typedef struct ls_worker_s {
    int epfd;
    ls_server_context_t* server;
    ls_connection_t** connections;
    size_t n_connections;
    size_t max_connections;
    ls_mem_pool_t* pool;
} ls_worker_t;

ls_worker_t* ls_create_worker(ls_server_context_t* server_context, size_t max_conections);
ls_lstning_sock_t* ls_add_listener(ls_worker_t* worker, ls_socket_conf_t socket_conf);
