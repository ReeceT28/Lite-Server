#pragma once
#include <stdint.h>
// CAN HAVE THESE TO SET SOME DEFAULT VALUES WHEN CONSTRUCTING MAYBE
typedef enum 
{
    LT_PARSE_STRICT,
    LT_PARSE_LENIENT,
    LT_PARSE_FAST
} lt_parse_mode;

/* Idea: Use flags in a bitfield to determine things about the server like https. strict parsing. keeping logs etc...*/
// You cannot have strict and 
#define LS_ENABLE_HTTPS    (1 << 1)
#define LS_LOG_HEADERS     (1 << 2)
#define LS_ENABLE_CORS     (1 << 3)
#define LS_KEEP_ALIVE      (1 << 4)
#define LS_ALLOW_CHUNKED   (1 << 5)
#define LS_ALLOW_BARELF    (1 << 6)
#define LS_ALLOW_LEADING_CRLF (1 << 9)
#define LS_ENFORCE_GRAMMAR (1 << 10)

typedef struct
{
    int port;
    int max_connections;
    uint32_t flags;
} lite_server_config;