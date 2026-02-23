#pragma once
#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>
#include <memory.h>
#include "server_config.h"

#if defined(__GNUC__) || defined(__clang__)
#define likely(x)   (__builtin_expect(!!(x), 1))
#define unlikely(x) (__builtin_expect(!!(x), 0))
#else
#define likely(x)   (x)
#define unlikely(x) (x)
#endif

#define SEND_BUF_SIZE 4096
#define MAX_REQUEST_HEADERS 16
#define MAX_LEADING_CRLF 6
#define IS_OWS(c) (c == ' ' || c == '\t')


#if defined(__BYTE_ORDER__) && (__BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__)
    #define LT_LITTLE_ENDIAN 1
#else
    #define LT_LITTLE_ENDIAN 0
#endif


/*
 * Possible advantages of unaligned memory access:
 * - Could allow faster checking of equivalence of strings like methods seen in nginx source code
 * Possible disadvantages of unaligned memory access:
 * - Can have performance penalties, will need to benchmark to test 
 * Notes:
 * - Linux has a Kconfig option called HAVE_EFFICIENT_UNALIGNED_ACCESS.
 * - This indicates if a platform can perform unaligned memory accesses efficieintly, I will use the same criteria as this flag.
 * Useful links:
 * - https://blog.vitlabuda.cz/2025/01/22/unaligned-memory-access-on-various-cpu-architectures.html
 * - https://git.kernel.org/pub/scm/linux/kernel/git/torvalds/linux.git/tree/arch/Kconfig?h=v6.13#n166
 * - https://www.kernel.org/doc/html/latest/core-api/unaligned-memory-access.html#alignment-vs-networking
 * Architectures that I have checked have efficient Unaligned memory access:
 * - x86/64
 * - i386
*/

#if defined(__i386__)     || \
    defined(__x86_64__)
    #define LS_HAVE_EFFICIENT_UNALIGNED_ACCESS 1
#else
    #define LS_HAVE_EFFICIENT_UNALIGNED_ACCESS 0
#endif

#if defined(__BYTE_ORDER__) && (__BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__)
    #define LS_IS_LITTLE_ENDIAN 1
#elif defined(__BYTE_ORDER__) && (__BYTE_ORDER__ == __ORDER_BIG_ENDIAN__)
    #define LS_IS_LITTLE_ENDIAN 0
#else
    #error "Cannot determine endian-ness for this architecture"
#endif

typedef unsigned char u_char;



enum {
    LS_HTTP_GET,
    LS_HTTP_PUT,
    LS_HTTP_HEAD,
    LS_HTTP_POST,
    LS_HTTP_TRACE,
    LS_HTTP_DELETE,
    LS_HTTP_CONNECT,
    LS_HTTP_OPTIONS,
    LS_HTTP_UNRECOGNISED_METHOD
};

/**
 *  RFC 9110 Section 9.1: 
 * "An origin server that receives a request method that is unrecognized or not implemented SHOULD respond with the 501 (Not Implemented) status code"
 * 
 */
enum {
    LS_ERR_BAD_REQUEST,
    LS_ERR_NOT_IMPLEMENTED,
    LS_ERR_NEED_MORE_CHARS
};

enum{
    LS_HTTP_METHOD,
    LS_HTTP_CUSTOM_HTTP_METHOD,
    LS_HTTP_SPACE_AFTER_METHOD,
    LS_HTTP_FIGURE_OUT_REQ_TARGET_TYPE,
    LS_HTTP_SCHEMA,
    LS_HTTP_SCHEMA_SLASH,
    LS_HTTP_SCHEMA_SLASH_SLASH,
    LS_HTTP_HOST_START,
    LS_HTTP_HOST_REG_NAME,
    LS_HTTP_HOST_IP_LITERAL,
    LS_HTTP_HOST_END,
    LS_HTTP_PORT,
    LS_HTTP_AFTER_SLASH_IN_PATH,
    LS_HTTP_HANDLE_QUERY,
    LS_HTTP_VERSION,
    LS_HTTP_END_OF_REQUEST_LINE
};

/** 
 * @brief Represents a HTTP header, used for http requests.
 */
typedef struct
{
    const u_char* name;     /**<  Name of header */
    size_t name_len;      /**<  Length of name string */
    const u_char* value;    /**< Value of header */
    size_t value_len;     /**< Length of value string */
} request_header;

/*
 * @brief represents a HTTP request.
 */
typedef struct
{
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
} http_request;



typedef const u_char* (*parse_func)(const u_char* cursor, const u_char* end, http_request* req, int* err_code, int* state);

/* REQUEST LINE FUNCTIONS */
const u_char* parse_request_line        (const u_char* cursor, const u_char* end, http_request* req, int* err_code, int* state);
const u_char* parse_method              (const u_char* cursor, const u_char* end, http_request* req, int* err_code, int* state);
const u_char* parse_custom_method       (const u_char* cursor, const u_char* end, http_request* req, int* err_code, int* state);
const u_char* parse_space_after_method  (const u_char* cursor, const u_char* end, http_request* req, int* err_code, int* state);
const u_char* parse_req_target_type     (const u_char* cursor, const u_char* end, http_request* req, int* err_code, int* state);
const u_char* parse_schema              (const u_char* cursor, const u_char* end, http_request* req, int* err_code, int* state);
const u_char* parse_schema_slash        (const u_char* cursor, const u_char* end, http_request* req, int* err_code, int* state);
const u_char* parse_schema_slash_slash  (const u_char* cursor, const u_char* end, http_request* req, int* err_code, int* state);
const u_char* parse_host_start          (const u_char* cursor, const u_char* end, http_request* req, int* err_code, int* state);
const u_char* parse_host_reg_name       (const u_char* cursor, const u_char* end, http_request* req, int* err_code, int* state);
const u_char* parse_host_ip_literal     (const u_char* cursor, const u_char* end, http_request* req, int* err_code, int* state);
const u_char* parse_host_end            (const u_char* cursor, const u_char* end, http_request* req, int* err_code, int* state);
const u_char* parse_port                (const u_char* cursor, const u_char* end, http_request* req, int* err_code, int* state);
const u_char* parse_after_slash_in_path (const u_char* cursor, const u_char* end, http_request* req, int* err_code, int* state);
const u_char* parse_handle_query        (const u_char* cursor, const u_char* end, http_request* req, int* err_code, int* state);
const u_char* parse_http_version        (const u_char* cursor, const u_char* end, http_request* req, int* err_code, int* state);
const u_char* parse_end_of_request_line (const u_char* cursor, const u_char* end, http_request* req, int* err_code, int* state);