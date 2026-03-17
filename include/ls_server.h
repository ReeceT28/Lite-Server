#pragma once
#include "ls_connection.h"
#include "ls_array.h"

typedef struct ls_server_context_s {
    ls_array_t* lstning_sockets;
    ls_array_t* connections;

} ls_server_context_t;
