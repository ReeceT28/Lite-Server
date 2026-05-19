#pragma once
#include "ls_array.h"
#include "ls_mem_pool.h"

typedef struct ls_log_cfg_s ls_log_cfg_t;

typedef struct ls_server_context_s {
    int epfd;
    ls_array_t* lstning_sockets;
    ls_array_t* connections;
    ls_mem_pool_t* pool;
    u_char* root;
    ls_log_cfg_t* log_cfg;
} ls_server_context_t;

void ls_set_root_dir(ls_server_context_t* server, const char* root);
