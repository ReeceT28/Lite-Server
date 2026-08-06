#pragma once
#include "ls_http_request.h"
#include "ls_mem_pool.h"
#include "ls_server.h"
#include <sys/types.h>
#define MAX_RESPONSE_SIZE 8 * 1024

typedef struct ls_http_response_s {
    int status;
    int file_fd; 
    int res_in_progress;
    off_t file_size;
    off_t file_offset;
    char* response;
    size_t response_size;
    size_t response_sent;
} ls_http_response_t;

/* Generic function pointer to a user supplied request handler */
typedef int (*ls_user_req_handler_ptr)(ls_http_request_t* req, ls_http_response_t* res, ls_mem_pool_t* pool);

typedef struct ls_resource_function_s {
    const char* resource;
    ls_user_req_handler_ptr user_function; 
} ls_resource_function_t; 

ls_http_response_t* ls_build_http_response(ls_mem_pool_t* pool, ls_http_request_t* req, ls_server_context_t* server);
ls_http_response_t* ls_build_simple_http_response(ls_mem_pool_t* pool, ls_http_request_t* req, int status, const char* text);
int ls_append(ls_http_response_t* res, const char* data, size_t len);
void ls_write_status_line(ls_http_response_t* res, int http_major, int http_minor, int status, const char* reason);
