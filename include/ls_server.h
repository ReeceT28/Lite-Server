#pragma once
#include "ls_array.h"

typedef struct ls_server_context_s {
    int epfd;
    ls_array_t* lstning_sockets;
    ls_array_t* connections;
} ls_server_context_t;
