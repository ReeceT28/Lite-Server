#pragma once
#include <stdlib.h>
#include "ls_types.h"

#define SEND_BUF_SIZE 4096
#define MAX_REQUEST_HEADERS 16



/** 
 * @brief Represents a HTTP header, used for http requests.
 */
typedef struct {
    const u_char* name;     /**<  Name of header */
    size_t name_len;      /**<  Length of name string */
    const u_char* value;    /**< Value of header */
    size_t value_len;     /**< Length of value string */
} request_header;

/*
 * @brief represents a HTTP request.
 */
typedef struct {
    int method;
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
    request_header headers[MAX_REQUEST_HEADERS];       /**< Array of request_headers */
    size_t header_count;                               /**< Number of headers currently in array */
    const u_char* raw_request;                           /**< Pointer to original raw request */
    size_t request_len;                                /**< Length of the original request */
    unsigned int http_minor;
    unsigned int http_major;
} ls_http_request_t;


typedef int (*ls_http_header_handler_ptr)(ls_http_request_t* req);

typedef struct { 
    const u_char* name;  /* Null terminated name       */
    unsigned int offset; /* Store offset in hash table */
     
    /* Add something to handle the header if needed    */
} ls_http_header_t;


