#pragma once
#include "ls_array.h"
#include "ls_mem_pool.h"

typedef struct ls_log_cfg_s ls_log_cfg_t;
typedef struct ls_connection_s ls_connection_t;

typedef struct ls_server_context_s {
    ls_array_t* lstning_sockets;
    u_char* root;
    ls_log_cfg_t* log_cfg;
    ls_mem_pool_t* pool;
} ls_server_context_t;

typedef struct ls_worker_s {
    int epfd;
    ls_server_context_t* server;
    ls_connection_t** connections;
    size_t n_connections;
    size_t max_connections;
    ls_mem_pool_t* pool;
} ls_worker_t;

void ls_set_root_dir(ls_server_context_t* server, const char* root);
