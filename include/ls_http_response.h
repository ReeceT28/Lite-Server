#pragma once
#include "ls_connection.h"
#define MAX_RESPONSE_SIZE 4 * 1024

typedef struct ls_http_response_s {
    int status;
    int file_fd; 
    off_t file_size;
    off_t file_offset;
    char* response;
    size_t response_size;
    size_t response_sent;
} ls_http_response_t;

ls_http_response_t* ls_build_http_response(ls_connection_t* conn);
void ls_send_http_response(ls_connection_t* conn, ls_http_response_t* response);
/* Maybe create function pointers for ls_send_http_response */