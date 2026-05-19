#pragma once
#include "ls_array.h"

typedef struct ls_server_context_s {
    int epfd;
    ls_array_t* lstning_sockets;
    ls_array_t* connections;
    u_char* root;
} ls_server_context_t;

void ls_set_root_dir(ls_server_context_t* server, const char* root);
