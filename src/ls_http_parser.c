#include <memory.h>
#include <stdint.h>
#include <stddef.h>
#include "ls_http_request.h"
#include "ls_http_parser.h"
#include "ls_trie.h"

#if (LS_IS_LITTLE_ENDIAN)
    #define LS_CHAR4_INT(a, b, c, d)					\
    	 ((d << 24) | (c << 16) | (b << 8) | a)
    #define LS_CHAR8_INT(a, b, c, d, e, f, g, h)				\
    	 (((long)h << 56) | ((long)g << 48) | ((long)f << 40)		\
    	  | ((long)e << 32) | (d << 24) | (c << 16) | (b << 8) | a)
#else
    #define LS_CHAR4_INT(a,b,c,d) \
        ((a << 24) | (b << 16) | (c << 8) | (d))
    #define LS_CHAR8_INT(a,b,c,d,e,f,g,h) \
        (((long)(a) << 56) | ((long)(b) << 48) | \
         ((long)(c) << 40) | ((long)(d) << 32) | \
         ((e) << 24) | ((f) << 16) | ((g) << 8) | (h))
#endif

static inline unsigned int P(const u_char* p)
{
#if (LS_HAVE_EFFICIENT_UNALIGNED_ACCESS)
    return (*(uint32_t *)(p));
#else
	return ((unsigned int)((p)[0]) | ((unsigned int)((p)[1]) << 8) |
		    ((unsigned int)((p)[2]) << 16) | ((unsigned int)((p)[3]) << 24));
#endif
}


/* RFC 3986 Section 3.1 states " scheme = ALPHA * ( ALPHA / DIGIT / "+" / "-" / "." ) "  */
static const u_char schema_chars[256] = {
    ['a' ... 'z'] = 1,
    ['A' ... 'Z'] = 1,
    ['0' ... '9'] = 1,
    ['+'] = 1,
    ['-'] = 1,
    ['.'] = 1
};

static const u_char method_chars[256] = {
    ['A' ... 'Z'] = 1,
    ['_'] = 1,
    ['-'] = 1,
};

/* RFC 3986 Section 3.2.2 states       reg-name    = *( unreserved / pct-encoded / sub-delims )  */
static const u_char reg_name_chars[256] = {
    /* unreserved: */
    ['a' ... 'z'] = 1,
    ['A' ... 'Z'] = 1,
    ['0' ... '9'] = 1,
    ['-'] = 1,
    ['.'] = 1,
    ['_'] = 1,
    ['~'] = 1,
    /* pct-encoded: */
    ['%'] = 1, /* pct-encoded = "%" HEXDIG HEXDIG   we already included HEXDIG in unreserved chars*/
    /* sub-delims */
    ['!'] = 1,
    ['$'] = 1,
    ['&'] = 1,
    ['\''] = 1,
    ['('] = 1,
    [')'] = 1,
    ['*'] = 1,
    ['+'] = 1,
    [','] = 1,
    [';'] = 1,
    ['='] = 1
};

static const u_char pchar_chars[256] = {
    /* unreserved: */
    ['a' ... 'z'] = 1,
    ['A' ... 'Z'] = 1,
    ['0' ... '9'] = 1,
    ['-'] = 1,
    ['.'] = 1,
    ['_'] = 1,
    ['~'] = 1,
    /* pct-encoded: */
    ['%'] = 1, /* pct-encoded = "%" HEXDIG HEXDIG   we already included HEXDIG in unreserved chars*/
    /* sub-delims */
    ['!'] = 1,
    ['$'] = 1,
    ['&'] = 1,
    ['\''] = 1,
    ['('] = 1,
    [')'] = 1,
    ['*'] = 1,
    ['+'] = 1,
    [','] = 1,
    [';'] = 1,
    ['='] = 1,
    /* Extra chars */
    [':'] = 1,
    ['@'] = 1,
};

static const u_char path_chars[256] = {
    /* unreserved: */
    ['a' ... 'z'] = 1,
    ['A' ... 'Z'] = 1,
    ['0' ... '9'] = 1,
    ['-'] = 1,
    ['.'] = 1,
    ['_'] = 1,
    ['~'] = 1,
    /* pct-encoded: */
    ['%'] = 1, /* pct-encoded = "%" HEXDIG HEXDIG   we already included HEXDIG in unreserved chars*/
    /* sub-delims */
    ['!'] = 1,
    ['$'] = 1,
    ['&'] = 1,
    ['\''] = 1,
    ['('] = 1,
    [')'] = 1,
    ['*'] = 1,
    ['+'] = 1,
    [','] = 1,
    [';'] = 1,
    ['='] = 1,
    /* Extra chars */
    [':'] = 1,
    ['@'] = 1,
    ['/'] = 1,
};

static const u_char ip_literal_chars[256] = {
    ['a' ... 'z'] = 1,
    ['A' ... 'Z'] = 1,
    ['0' ... '9'] = 1,
    [':'] = 1,
    /* Can add more if want support for IPvFuture */
};

static const unsigned char header_chars[256] = {
    [0 ... 255] = 255,
    /* Uppercase maps to lowercase */
    ['A'] = 0, ['B'] = 1, ['C'] = 2, ['D'] = 3,
    ['E'] = 4, ['F'] = 5, ['G'] = 6, ['H'] = 7,
    ['I'] = 8, ['J'] = 9, ['K'] = 10, ['L'] = 11,
    ['M'] = 12, ['N'] = 13, ['O'] = 14, ['P'] = 15,
    ['Q'] = 16, ['R'] = 17, ['S'] = 18, ['T'] = 19,
    ['U'] = 20, ['V'] = 21, ['W'] = 22, ['X'] = 23,
    ['Y'] = 24, ['Z'] = 25,
    /* Lowercase map to themselves */
    ['a'] = 0, ['b'] = 1, ['c'] = 2, ['d'] = 3,
    ['e'] = 4, ['f'] = 5, ['g'] = 6, ['h'] = 7,
    ['i'] = 8, ['j'] = 9, ['k'] = 10, ['l'] = 11,
    ['m'] = 12, ['n'] = 13, ['o'] = 14, ['p'] = 15,
    ['q'] = 16, ['r'] = 17, ['s'] = 18, ['t'] = 19,
    ['u'] = 20, ['v'] = 21, ['w'] = 22, ['x'] = 23,
    ['y'] = 24, ['z'] = 25,
    /* Digits map to themselves */
    ['0'] = 26, ['1'] = 27, ['2'] = 28, ['3'] = 29,
    ['4'] = 30, ['5'] = 31, ['6'] = 32, ['7'] = 33,
    ['8'] = 34, ['9'] = 35,
    /* Dash maps to itself */
    ['-'] = 36,
};

/* === Start of funtions for the request line  === */

static inline const u_char* parse_method(const u_char* cursor, const u_char* end, ls_http_request_t* req, int* err_code, int* state)
{
    /**
     * The shortest HTTP request is: "GET / HTTP/1.1\n\n" which is 16 characters long if we allow \n instead of \r\n.
     * The max amount we check for a method is 8 so we can check this condition to ensure the request is valid and all methods can be safely checked.
     * Without testing I think it is also unlikely that we would read less than 8 bytes of a request to start with which is why I think this is a good optimisation but I can't benchmark this
     * until I get my webserver up and running better
     */
    if(unlikely(cursor + 8 >= end)) {
        *err_code = LS_ERR_NEED_MORE_CHARS;
        return cursor;
    }
    /* Fast track for GET and POST which are by far the most common methods */
    if(likely(P(cursor) == LS_CHAR4_INT('G','E','T',' '))) {
        req->method = LS_HTTP_GET;
        cursor += 4;
        *state = LS_HTTP_FIGURE_OUT_REQ_TARGET_TYPE;
        return cursor;
    } else if(likely(P(cursor) == LS_CHAR4_INT('P','O','S','T'))) {
        req->method = LS_HTTP_POST;
        cursor += 4;
        *state = LS_HTTP_SPACE_AFTER_METHOD;
        return cursor;
    } else {
        /* Handle the more uncommon methods */
        switch(P(cursor))
        {
            case LS_CHAR4_INT('P','U','T',' '):
                req->method = LS_HTTP_PUT;
                cursor += 4;
                *state = LS_HTTP_FIGURE_OUT_REQ_TARGET_TYPE;
                return cursor;
            case LS_CHAR4_INT('H','E','A','D'):
                req->method = LS_HTTP_HEAD;
                cursor += 4;
                *state = LS_HTTP_SPACE_AFTER_METHOD;
                return cursor;
            case LS_CHAR4_INT('D','E','L','E'):
                if(*(cursor+4)=='T' && *(cursor+5)=='E') {
                    req->method = LS_HTTP_DELETE;
                    cursor += 6;
                    *state = LS_HTTP_SPACE_AFTER_METHOD;
                    return cursor;
                }
                *err_code = LS_ERR_BAD_REQUEST;
                return NULL;
            case LS_CHAR4_INT('C','O','N','N'):
                if(P(cursor+4) == LS_CHAR4_INT('E','C','T',' ')) {
                    req->method = LS_HTTP_CONNECT;
                    cursor += 8;
                    req->host_start = cursor;
                    *state = LS_HTTP_HOST_START;
                    return cursor;
                }
                *err_code = LS_ERR_BAD_REQUEST;
                return NULL;
            case LS_CHAR4_INT('O','P','T','I'):
                if(P(cursor+4) == LS_CHAR4_INT('O','N','S',' ')) {
                    req->method = LS_HTTP_OPTIONS;
                    cursor += 8;
                    *state = LS_HTTP_FIGURE_OUT_REQ_TARGET_TYPE;
                    return cursor;
                }
                *err_code = LS_ERR_BAD_REQUEST;
                return NULL;
            case LS_CHAR4_INT('T','R','A','C'):
                if(*(cursor+4)=='E') {
                    req->method = LS_HTTP_TRACE;
                    cursor += 5;
                    *state = LS_HTTP_SPACE_AFTER_METHOD;
                    return cursor;
                }
                *err_code = LS_ERR_BAD_REQUEST;
                return NULL;
            default:
                req->method_start = cursor;
                *state = LS_HTTP_CUSTOM_HTTP_METHOD;
                return cursor;
        }
    }
}

static inline const u_char* parse_custom_method(const u_char* cursor, const u_char* end, ls_http_request_t* req, int* err_code, int* state)
{
    while(cursor < end && method_chars[*cursor])
        ++cursor;
    if(cursor >= end) {
        *err_code = LS_ERR_NEED_MORE_CHARS;
        *state = LS_HTTP_CUSTOM_HTTP_METHOD;
        return cursor;
    }
    if(*cursor != ' ') {
        *err_code = LS_ERR_BAD_REQUEST;
        return NULL;
    }
    *state = LS_HTTP_FIGURE_OUT_REQ_TARGET_TYPE;
    req->method_end = cursor;
    return cursor;
}

static inline const u_char* parse_space_after_method(const u_char* cursor, const u_char* end, int* err_code, int* state)
{
    if(unlikely(*cursor != ' ')) {
        *err_code = LS_ERR_BAD_REQUEST;
        return NULL;
    }
    if(cursor >= end) {
        *err_code = LS_ERR_NEED_MORE_CHARS;
        *state = LS_HTTP_SPACE_AFTER_METHOD;
        return cursor;
    }
    ++cursor;
    *state = LS_HTTP_FIGURE_OUT_REQ_TARGET_TYPE;
    return cursor;
}

static inline const u_char* parse_req_target_type(const u_char* cursor, const u_char* end, ls_http_request_t* req, int* err_code, int* state)
{
    if(cursor >= end) {
        *err_code = LS_ERR_NEED_MORE_CHARS;
        *state = LS_HTTP_FIGURE_OUT_REQ_TARGET_TYPE;
        return cursor;
    }
    if(*cursor == '/') {
        req->path_start = cursor;
        ++cursor;
        *state = LS_HTTP_AFTER_SLASH_IN_PATH;
        return cursor;
    }
    /* OR with 0x20 converts upper to lower and keeps lower, lower */
    /* a <= ch <= z   ->   0 <= ch - 'a' <= 'z' - 'a' */
    /* if ch - 'a' is negative it underflows it will always go larger than 'z' - 'a' anyway */
    if(((*cursor | 0x20) - 'a') <= 'z'-'a') {
        *state = LS_HTTP_SCHEMA;
        return cursor;
    }
    *err_code = LS_ERR_BAD_REQUEST;
    return NULL;
}

/* --- schema parsing --- */
static inline const u_char* parse_schema(const u_char* cursor, const u_char* end, ls_http_request_t* req, int* err_code, int* state)
{
    req->schema_start = cursor;
    while(cursor < end && schema_chars[*cursor]) ++cursor;
    if(likely(*cursor == ':')) {
        req->schema_end = cursor++;
        *state = LS_HTTP_SCHEMA_SLASH;
        return cursor;
    }
    if(cursor >= end) {
        *err_code = LS_ERR_NEED_MORE_CHARS;
        *state = LS_HTTP_SCHEMA;
        return cursor;
    }
    *err_code = LS_ERR_BAD_REQUEST;
    return NULL;
}

static inline const u_char* parse_schema_slash(const u_char* cursor, const u_char* end, int* err_code, int* state)
{
    if(likely(*cursor == '/')) {
        ++cursor;
        *state = LS_HTTP_SCHEMA_SLASH_SLASH;
        return cursor;
    }
    if(cursor >= end) {
        *err_code = LS_ERR_NEED_MORE_CHARS;
        *state = LS_HTTP_SCHEMA_SLASH;
        return cursor;
    }
    *err_code = LS_ERR_BAD_REQUEST;
    return NULL;
}

static inline const u_char* parse_schema_slash_slash(const u_char* cursor, const u_char* end, int* err_code, int* state)
{
    if(likely(*cursor == '/')) {
        ++cursor;
        *state = LS_HTTP_HOST_START;
        return cursor;
    }
    if(cursor >= end) {
        *err_code = LS_ERR_NEED_MORE_CHARS;
        *state = LS_HTTP_SCHEMA_SLASH_SLASH;
        return cursor;
    }
    *err_code = LS_ERR_BAD_REQUEST;
    return NULL;
}

/* --- host parsing --- */
static inline const u_char* parse_host_start(const u_char* cursor, const u_char* end, ls_http_request_t* req, int* err_code, int* state)
{
    /* RFC 3986 Section 3.2.2 states       host = IP-literal / IPv4address / reg-name                */
    /* I let IPv4 be processed as reg_name as reg_name can contain digits and '.'        */
    req->host_start = cursor;
    if(unlikely(*cursor=='[')) {
        ++cursor;
        *state = LS_HTTP_HOST_IP_LITERAL;
        return cursor;
    }
    if(cursor >= end) {
        *err_code = LS_ERR_NEED_MORE_CHARS;
        *state = LS_HTTP_HOST_START;
        return cursor;
    }
    *state = LS_HTTP_HOST_REG_NAME;
    return cursor;
}

static inline const u_char* parse_host_reg_name(const u_char* cursor, const u_char* end, ls_http_request_t* req, int* err_code, int* state)
{
    /* RFC 3986 Section 3.2.2 states       reg-name    = *( unreserved / pct-encoded / sub-delims )  */
    while(cursor < end && reg_name_chars[*cursor]) ++cursor;
    if(cursor >= end) {
        *err_code = LS_ERR_NEED_MORE_CHARS;
        *state = LS_HTTP_HOST_REG_NAME;
        return cursor;
    }
    req->host_end = cursor;
    *state = LS_HTTP_HOST_END;
    return cursor;
}

static inline const u_char* parse_host_ip_literal(const u_char* cursor, const u_char* end, ls_http_request_t* req, int* err_code, int* state)
{
    /* We only really need to support IPv6 literals. IPvFuture doesn't really exist. IPv4 is handled as a regular host_reg_name */
    while(cursor < end && ip_literal_chars[*cursor]) ++cursor;
    if(unlikely(*cursor != ']')) {
        *err_code = LS_ERR_BAD_REQUEST;
        return NULL;
    }
    if(cursor >= end) {
        *err_code = LS_ERR_NEED_MORE_CHARS;
        *state = LS_HTTP_HOST_IP_LITERAL;
        return cursor;
    }
    ++cursor;
    req->host_end = cursor;
    *state = LS_HTTP_HOST_END;
    return cursor;
}

static inline const u_char* parse_host_end(const u_char* cursor, const u_char* end, ls_http_request_t* req, int* err_code, int* state)
{
    if(*cursor == ':') {
        req->port_start = ++cursor;
        *state = LS_HTTP_PORT;
        return cursor;
    }

    /* Here we handle all cases of absolute form but a path-rootless (I can't see any use cases and neither does nginx it seems) */
    if(*cursor == '/') {
        /* absolute form && ( (path-abempty && not empty) || path-absolute) */
        req->path_start = cursor++;
        *state = LS_HTTP_AFTER_SLASH_IN_PATH;
        return cursor;
    }
    if(*cursor == '?') {
        /* asbolute form && (path-empty || (path-abempty && empty))  */
        req->query_start = ++cursor;
        *state = LS_HTTP_HANDLE_QUERY;
        return cursor;
    }
    if(*cursor == ' ') {
        *state = LS_HTTP_VERSION;
        return ++cursor;
    }
    if(cursor >= end) {
        *err_code = LS_ERR_NEED_MORE_CHARS;
        *state = LS_HTTP_HOST_END;
        return cursor;
    }
    *err_code = LS_ERR_BAD_REQUEST;
    return NULL;
}

/* --- port parsing --- */
static inline const u_char* parse_port(const u_char* cursor, const u_char* end, ls_http_request_t* req, int* err_code, int* state)
{
    while(cursor < end && *cursor >= '0' && *cursor <= '9') ++cursor;
    req->port_end = cursor;

    if(*cursor == '/') {
        req->path_start = cursor++;
        *state = LS_HTTP_AFTER_SLASH_IN_PATH;
        return cursor;
    }
    if(*cursor == '?') {
        req->query_start = ++cursor;
        *state = LS_HTTP_HANDLE_QUERY;
        return cursor;
    }
    if(*cursor == ' ') {
        ++cursor;
        *state = LS_HTTP_VERSION;
        return cursor;
    }
    if(cursor >= end) {
        *err_code = LS_ERR_NEED_MORE_CHARS;
        *state = LS_HTTP_PORT;
        return cursor;
    }
    *err_code = LS_ERR_BAD_REQUEST;
    return NULL;
}

/* --- path parsing --- */
static inline const u_char* parse_after_slash_in_path(const u_char* cursor, const u_char* end, ls_http_request_t* req, int* err_code, int* state)
{
    while(cursor < end && path_chars[*cursor])
        ++cursor;

    req->path_end = cursor;

    if(cursor >= end) {
        *err_code = LS_ERR_NEED_MORE_CHARS;
        *state = LS_HTTP_AFTER_SLASH_IN_PATH;
        return cursor;
    }
    req->path_end = cursor;

    if(*cursor == '?') {
        req->query_start = ++cursor;
        *state = LS_HTTP_HANDLE_QUERY;
        return cursor;
    }
    if(*cursor == ' ') {
        req->version_start = ++cursor;
        *state = LS_HTTP_VERSION;
        return cursor;
    }
    *err_code = LS_ERR_BAD_REQUEST;
    return NULL;
}

/* --- query parsing --- */
static inline const u_char* parse_handle_query(const u_char* cursor, const u_char* end, ls_http_request_t* req, int* err_code, int* state)
{
    /* RFC 3986 Section 3.4 states  " query = *( pchar / "/" / "?" ) "  */
    while(cursor < end && (pchar_chars[*cursor] || *cursor=='/' || *cursor=='?')) ++cursor;
    req->query_end = cursor;
    /* Query should be last part of request-target */
    if(*cursor == ' ') {
        req->version_start = ++cursor;
        *state = LS_HTTP_VERSION;
        return cursor;
    }
    if(cursor >= end) {
        *err_code = LS_ERR_NEED_MORE_CHARS;
        *state = LS_HTTP_HANDLE_QUERY;
        return cursor;
    }
    *err_code = LS_ERR_BAD_REQUEST;
    return NULL;
}

/* --- HTTP version parsing --- */
static inline const u_char* parse_http_version(const u_char* cursor, const u_char* end, ls_http_request_t* req, int* err_code, int* state)
{

    /*min now:  "HTTP/x.y\n\n\0" so min of end-cursor = 10 */
    if(end - cursor < 10) {
        *err_code = LS_ERR_NEED_MORE_CHARS;
        *state = LS_HTTP_VERSION;
        return cursor;
    }

    if(likely(P(cursor) == LS_CHAR4_INT('H','T','T','P'))) {
        /* We have enough clearance in this section from check at start to ensure we don't go out of bounds */
        cursor += 4;
        if(unlikely(*cursor != '/')) { *err_code = LS_ERR_BAD_REQUEST; return NULL; }
        ++cursor;
        req->http_major = *cursor - '0';
        if(req->http_major > 9) { *err_code = LS_ERR_BAD_REQUEST; return NULL; }
        ++cursor;
        if(unlikely(*cursor != '.')) { *err_code = LS_ERR_BAD_REQUEST; return NULL; }
        ++cursor;
        req->http_minor = *cursor - '0';
        if(req->http_minor > 9) { *err_code = LS_ERR_BAD_REQUEST; return NULL; }
        *state = LS_HTTP_END_OF_REQUEST_LINE;
        return ++cursor;
    }
    *err_code = LS_ERR_BAD_REQUEST;
    return NULL;
}

static inline const u_char* parse_end_of_request_line(const u_char* cursor, const u_char* end, int* err_code, int* state)
{
    if(*cursor == '\r') {
        ++cursor;
        if(likely(*cursor == '\n')) {
            /* In the event that cursor = end after this increment it will be caught when parsing headers and ask for more chars*/
            ++cursor;
            *state = LS_HTTP_REQ_LINE_DONE;
            return cursor;
        }
        if(cursor >= end) {
            *err_code = LS_ERR_NEED_MORE_CHARS;
            return cursor-1;
        }
    }
    if(*cursor == '\n') {
        ++cursor;
        *state = LS_HTTP_REQ_LINE_DONE;
        return cursor;
    }
    *err_code = LS_ERR_BAD_REQUEST;
    return NULL;
}

const u_char* parse_request_line(const u_char* cursor, const u_char* end, ls_http_request_t* req, int* err_code, int* state)
{
    if (!state) return NULL;
    while (1) {
        const u_char* next = NULL;
        switch (*state) {
            case LS_HTTP_METHOD:
                next = parse_method(cursor, end, req, err_code, state);
                break;
            case LS_HTTP_CUSTOM_HTTP_METHOD:
                next = parse_custom_method(cursor, end, req, err_code, state);
                break;
            case LS_HTTP_SPACE_AFTER_METHOD:
                next = parse_space_after_method(cursor, end, err_code, state);
                break;
            case LS_HTTP_FIGURE_OUT_REQ_TARGET_TYPE:
                next = parse_req_target_type(cursor, end, req, err_code, state);
                break;
            case LS_HTTP_SCHEMA:
                next = parse_schema(cursor, end, req, err_code, state);
                break;
            case LS_HTTP_SCHEMA_SLASH:
                next = parse_schema_slash(cursor, end, err_code, state);
                break;
            case LS_HTTP_SCHEMA_SLASH_SLASH:
                next = parse_schema_slash_slash(cursor, end, err_code, state);
                break;
            case LS_HTTP_HOST_START:
                next = parse_host_start(cursor, end, req, err_code, state);
                break;
            case LS_HTTP_HOST_REG_NAME:
                next = parse_host_reg_name(cursor, end, req, err_code, state);
                break;
            case LS_HTTP_HOST_IP_LITERAL:
                next = parse_host_ip_literal(cursor, end, req, err_code, state);
                break;
            case LS_HTTP_HOST_END:
                next = parse_host_end(cursor, end, req, err_code, state);
                break;
            case LS_HTTP_PORT:
                next = parse_port(cursor, end, req, err_code, state);
                break;
            case LS_HTTP_AFTER_SLASH_IN_PATH:
                next = parse_after_slash_in_path(cursor, end, req, err_code, state);
                break;
            case LS_HTTP_HANDLE_QUERY:
                next = parse_handle_query(cursor, end, req, err_code, state);
                break;
            case LS_HTTP_VERSION:
                next = parse_http_version(cursor, end, req, err_code, state);
                break;
            case LS_HTTP_END_OF_REQUEST_LINE:
                next = parse_end_of_request_line(cursor, end, err_code, state);
                if (*state == LS_HTTP_REQ_LINE_DONE) {
                    cursor = next;
                    goto done;
                }
                break;
            default:
                return NULL;
        }
        if (!next)
        {
            return NULL;
        }

        if (*err_code == LS_ERR_NEED_MORE_CHARS)
            return next;

        cursor = next;
    }
done:
    return cursor;
}

/* === End of funtions for the request line  === */

/* === Start of funtions for the header lines  === */

static ls_trie_node_t* global_header_trie_root = NULL;

void ls_http_parser_init()
{
    global_header_trie_root = ls_http_init_header_trie();
}

/* This function is really bad for cache misses but every time I try to improve it becomes slower anyway */
static inline const u_char* parse_header_name(const u_char* cursor, const u_char* end, ls_http_request_t* req,
    int* err_code, int* state)
{
    while (cursor < end) {

        u_char c = header_chars[*cursor];

        if(c == 255) {
            if (likely(*cursor == ':'))
            {
                if (likely(req->current_trie_node != NULL)) {
                    req->header_id = req->current_trie_node->header_id;
                }
                else {
                    req->header_id = LS_HTTP_HDR_UNKOWN;
                }
                *state = LS_HTTP_OWS_BEFORE_VALUE;
                req->header_name_end = cursor;
                return ++cursor; /* move past ':' */
            }
            *err_code = LS_ERR_BAD_REQUEST;
            return NULL;
        }
        if (req->current_trie_node) {
            req->current_trie_node = req->current_trie_node->children[c];
        }

        ++cursor;
    }

    *err_code = LS_ERR_NEED_MORE_CHARS;
    return cursor;
}


static inline const u_char* parse_ows(const u_char* cursor, const u_char* end, ls_http_request_t* req,
    int* err_code, int* state)
{
    while(cursor < end && (*cursor == ' ' || *cursor == '\t'))
        ++cursor;
    if(cursor >= end)
    {
        *err_code = LS_ERR_NEED_MORE_CHARS;
        return cursor;
    }
    req->header_value_start = cursor;
    *state = LS_HTTP_HEADER_VALUE;
    return cursor;
}

static inline const u_char* parse_header_value(const u_char* cursor, const u_char* end, ls_http_request_t* req,
    int* err_code, int* state)
{
    while(cursor < end) {
        if(*cursor > 'r') {
            ++cursor;
            continue;
        }
        if(*cursor == '\r') {
            req->header_value_end = cursor;
            ++cursor;
            if(*cursor == '\n') {
                *state = LS_HTTP_STRIP_VALUE_OWS;
                return cursor;
            }
            if(cursor >= end) {
                *err_code = LS_ERR_NEED_MORE_CHARS;
                return cursor-1;
            }
        }
        else if(*cursor == '\n') {
            req->header_value_end = cursor;
            *state = LS_HTTP_STRIP_VALUE_OWS;
            return cursor;
        }
        else if(*cursor == '\0') {
            *err_code = LS_ERR_BAD_REQUEST;
            return NULL;
        }
        ++cursor;
    }
    *err_code = LS_ERR_NEED_MORE_CHARS;
    return cursor;
}

static inline void strip_value_ows(ls_http_request_t* req)
{
    /* End points to the position after the actual end so \r or \n so reduce by 1 */
    const u_char* curr_end = req->header_value_end - 1;
    while(curr_end > req->raw_request && (*curr_end == ' ' || *curr_end == '\t'))
        --curr_end;
    req->header_value_end = curr_end + 1;

}

static inline const u_char* parse_header_line( const u_char* cursor, const u_char* end, ls_http_request_t* req,
    int* err_code, int* state)
{
    while (1) {
        const u_char* next = NULL;
        switch (*state) {
            case LS_HTTP_HEADER_NAME:
                next = parse_header_name(cursor, end, req, err_code, state);
                break;
            case LS_HTTP_OWS_BEFORE_VALUE:
                next = parse_ows(cursor, end, req, err_code, state);
                break;
            case LS_HTTP_HEADER_VALUE:
                next = parse_header_value(cursor, end, req, err_code, state);
                break;
            case LS_HTTP_STRIP_VALUE_OWS:
                strip_value_ows(req);
                goto done;
            default:
                return NULL;
        }
        if (!next) {
            return NULL;
        }
        if (*err_code == LS_ERR_NEED_MORE_CHARS)
            return next;
        cursor = next;
    }
done:
    /* Cursor will be pointing to \n so move on 1 to go to next header line */
    return ++cursor;
}

const u_char* parse_header_lines(const u_char* cursor, const u_char* end, ls_http_request_t* req, int* err_code, int* state)
{
    /* If user wants to add their own header/field name then we can add it to the trie.
     * this can be done at runtime as well so wouldn't require recompilation  */
    while(1) {
        req->header_name_start = cursor;
        req->current_trie_node = global_header_trie_root;
        req->header_hash = 0xFFFFFFFF;
        cursor = parse_header_line(cursor, end, req, err_code, state);
        if(!store_header(req)) {
            return NULL;
        }
        if(*err_code != LS_ERR_OKAY) {
            return NULL;
        }
        if(*cursor == '\r') {
            if(*(++cursor) == '\n') {
                return cursor;
            }
            *err_code = LS_ERR_BAD_REQUEST;
            return NULL;
        }
        if(*cursor == '\n') {
            return cursor;
        }
        *state = LS_HTTP_HEADER_NAME;
    }
}

/* === End of functions for the header lines === */

const u_char* ls_http_parse_request(const u_char *cursor, const u_char *end, ls_http_request_t* req,
    int *err_code, int *state)
{
    cursor = parse_request_line(cursor, end, req, err_code, state);
    if(!cursor) {
        return NULL;
    }
    *state = LS_HTTP_HEADER_NAME;
    cursor = parse_header_lines(cursor, end, req, err_code, state);
    // parse_header_lines(cursor, end, req, err_code, state);
    return cursor;
}

// Research of URI parsing:
/* RFC 9112 Section 3.2 states       " request-target = origin-form / absolute-form / authority-form / asterisk-form "          */
/* RFC 9112 Section 3.2.1 states     " origin-form = absolute-path [ "?" query ] "                                              */
/* RFC 9110 Section 4.1 states       " absolute-path = 1*( "/" segment )"                                                       */
/* RFC 3986 Section 3.3 states       " segment = *pchar "                                                                       */
/* RFC 3986 Section 3.3 states       " pchar = unreserved / pct-encoded / sub-delims / ":" / "@" "                              */
/* RFC 3986 Section 2.3 states       " unreserved = ALPHA / DIGIT / "-" / "." / "_" / "~" "                                     */
/* RFC 3986 Section 2.1 states       " pct-encoded = "%" HEXDIG HEXDIG "                                                        */
/* RFC 3986 Section 2.1 states       " sub-delims = "!" / "$" / "&" / "'" / "(" / ")" / "*" / "+" / "," / ";" / "=" "           */
/* Okay so we can figure out what an absolute-path is now, next we need the query of an origin-form                             */
/* RFC 3986 Section 3.4 states       " query = *( pchar / "/" / "?" ) "                                                         */
/**
 * We can put together an origin-form request now:
 * origin-form = 1*( "/" segment ) [ "?" query ]
 * Where segment and query are just certain sets of restricted characters
*/
/* RFC 9112 Section 3.2.2 states     " absolute-form = absolute-URI "                                                           */
/* RFC 3986 Section 4.3 states       " absolute-URI = scheme ":" hier-part [ "?" query ] "                                      */
/* RFC 3986 Section 3.1 states       " scheme = ALPHA *( ALPHA / DIGIT / "+" / "-" / "." ) "                                    */
/* RFC 3986 Section 3.0 states       " hier-part = "//" authority path-abempty / path-absolute / path-rootless / path-empty "   */
/* Now we need to figure out what all these combinations of hier-part can be for absolyte URI                                   */
// NGINX ignores user info
/* RFC 3986 Section 3.2 states       " authority = [ userinfo "@" ] host [ ":" port ] "*/
/* RFC 3986 Section 3.2.1 states     " userinfo = *( unreserved / pct-encoded / sub-delims / ":" ) "                            */
/* userinfo adds no new characters as it is just pchar without "@" */
/* RFC 3986 Section 3.2.2 states     " host = IP-literal / IPv4address / reg-name "                                             */
/* RFC 3986 Section 3.2.2 states     " IP-literal = "[" ( IPv6address / IPvFuture ) "]" "                                       */
/* RFC 3986 Section 3.2.2 states     " IPvFuture  = "v" 1*HEXDIG "." 1*( unreserved / sub-delims / ":" ) "                      */
/* RFC 3986 Section 3.3 states       "    path-abempty  = *( "/" segment )" */
/* RFC 3986 Section 3.2.2 states       reg-name    = * ( unreserved / pct-encoded / sub-delims )  */
/**
 * So absolute-form = scheme ":" hier-part
 * Scheme is just a certain set of restricted characters
 * Authority
 */

/* MAYBE JUST ALLOW ANY unreserved, resered of pct-encoded*/
/* Maybe just allow any VCHAR */
