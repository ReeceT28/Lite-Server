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
    /* NO IDEA WHAT TO CALL THIS SHIT will change its name probably */
    ls_array_t* user_functions;
    int root_fd;
 } ls_server_context_t;


void ls_set_root_dir(ls_server_context_t* server, const char* root);
ls_server_context_t* ls_create_server(uint32_t log_cfgs, const char* log_file, const char* root_dir);
int ls_add_user_function(ls_server_context_t* server, const char* resource);
