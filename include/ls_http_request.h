#pragma once
#include <stdlib.h>
#include <stdio.h>
#include "ls_array.h"
#include "ls_types.h"

typedef struct ls_trie_node_s ls_trie_node_t;

#define SEND_BUF_SIZE 4096
#define LS_MAX_REQUEST_HEADERS 16
#define LS_HTTP_MAX_HEADER_NAME_LENGTH 32

enum {
    LS_HTTP_HDR_UNKOWN,
    LS_HTTP_HDR_HOST,
    LS_HTTP_HDR_ACCEPT,
    LS_HTTP_HDR_CONNECTION,
    LS_HTTP_HDR_REFERER,
    LS_HTTP_HDR_COOKIE,
    LS_HTTP_HDR_ORIGIN,
    LS_HTTP_HDR_USER_AGENT,
    LS_HTTP_HDR_ACCEPT_ENCODING,
    LS_HTTP_HDR_ACCEPT_LANGUAGE,
    LS_HTTP_HDR_CONTENT_TYPE,
    LS_HTTP_HDR_CONTENT_LENGTH,
    LS_HTTP_HDR_KEEP_ALIVE,
    LS_HTTP_HDR_AUTHORIZATION
};

/** 
 * @brief Represents a HTTP header, used for http requests.
 */
typedef struct {
    const u_char* name;     /**<  Name of header */
    size_t name_len;      /**<  Length of name string */
    const u_char* value;    /**< Value of header */
    size_t value_len;     /**< Length of value string */
} request_header;

typedef struct {
    const u_char* name_start;
    const u_char* name_end;
    const u_char* value_start;
    const u_char* value_end;
    unsigned int header_id;
} ls_header_t; 

/*
 * @brief represents a HTTP request.
 */
typedef struct {
    ls_array_t* headers;
    unsigned int header_capacity;
    int method;
    int state;
    const u_char* cursor;
    const u_char* method_start;
    const u_char* method_end;
    const u_char* schema_start;
    const u_char* schema_end;
    const u_char* host_start;
    const u_char* host_end;
    const u_char* port_start;
    const u_char* port_end;
    const u_char* query_start;
    const u_char* query_end;
    const u_char* path_start;                                  /**< Path of the request */
    const u_char* path_end;                                   /**< Length of the path string */
    const u_char* version_start;                               /**<  The HTTP version */
    const u_char* version_end;                                /**< Length of the version string */
    u_char* raw_request;                           /**< Pointer to original raw request */
    size_t request_len;                                /**< Length of the original request */
    size_t max_request_size;
    const u_char* header_name_start;
    const u_char* header_name_end; 
    const u_char* header_value_start;
    const u_char* header_value_end;
    int header_id;
    unsigned long header_hash;
    /* traversal pointer */
    ls_trie_node_t* current_trie_node;
    unsigned int http_minor;
    unsigned int http_major;
} ls_http_request_t;

int store_header(ls_http_request_t *req);
typedef int (*ls_http_header_handler_ptr)(ls_http_request_t* req);

ls_http_request_t* ls_create_request(ls_mem_pool_t* pool);
