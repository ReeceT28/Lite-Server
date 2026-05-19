#include <string.h>
#include "ls_server.h"


void ls_set_root_dir(ls_server_context_t* server, const char* root)
{
    server->root = (u_char*)root;
}