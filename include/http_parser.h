#pragma once
#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>
#include <memory.h>
#include "server_config.h"
#include "ls_types.h"

#if defined(__GNUC__) || defined(__clang__)
    #define likely(x)   (__builtin_expect((x), 1))
    #define unlikely(x) (__builtin_expect((x), 0))
#else
    #define likely(x)   (x)
    #define unlikely(x) (x)
#endif

#if defined(__BYTE_ORDER__) && (__BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__)
    #define LT_LITTLE_ENDIAN 1
#else
    #define LT_LITTLE_ENDIAN 0
#endif

/*
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

#if defined(__i386__) || defined(__x86_64__)
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
    LS_ERR_OKAY,
    LS_ERR_BAD_REQUEST,
    LS_ERR_NOT_IMPLEMENTED,
    LS_ERR_NEED_MORE_CHARS
};

enum {
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
    LS_HTTP_END_OF_REQUEST_LINE,
    LS_HTTP_REQ_LINE_DONE
};

enum {
    LS_HTTP_HEADER_NAME,
    LS_HTTP_OWS_BEFORE_VALUE,
    LS_HTTP_HEADER_VALUE,
    LS_HTTP_STRIP_VALUE_OWS,
    LS_HTTP_END_OF_HEADERS
};

const u_char* parse_request_line(const u_char* cursor, const u_char* end, ls_http_request_t* req, 
    int* err_code, int* state);

const u_char* ls_http_parse_request(const u_char* cursor, const u_char* end, ls_http_request_t* req,
    int* err_code, int* state);

const u_char* parse_header_lines(const u_char *cursor, const u_char *end, ls_http_request_t* req,
    int *err_code, int* state);

    
void ls_http_parser_init();