#pragma once
#include "ls_http_request.h"
#include "ls_mem_pool.h"
#include "ls_server.h"
#include <sys/types.h>
#define MAX_RESPONSE_SIZE 8 * 1024

typedef struct ls_http_response_s {
    int status;
    int file_fd; 
    off_t file_size;
    off_t file_offset;
    char* response;
    size_t response_size;
    size_t response_sent;
} ls_http_response_t;

ls_http_response_t* ls_build_http_response(ls_mem_pool_t* pool, ls_http_request_t* req, ls_server_context_t* server);
/* Maybe create function pointers for ls_send_http_response */