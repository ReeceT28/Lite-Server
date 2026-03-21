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

static inline int parse_method(const u_char* cursor, const u_char* end, ls_http_request_t* req)
{
    /**
     * The shortest HTTP request is: "GET / HTTP/1.1\n\n" which is 16 characters long if we allow \n instead of \r\n.
     * The max amount we check for a method is 8 so we can check this condition to ensure the request is valid and all methods can be safely checked.
     * Without testing I think it is also unlikely that we would read less than 8 bytes of a request to start with which is why I think this is a good optimisation but I can't benchmark this
     * until I get my webserver up and running better
     */
    if(unlikely(cursor + 8 >= end)) {
        return LS_ERR_NEED_MORE_CHARS;
    }
    /* Fast track for GET and POST which are by far the most common methods */
    if(likely(P(cursor) == LS_CHAR4_INT('G','E','T',' '))) {
        req->method = LS_HTTP_GET;
        req->cursor = cursor + 4;
        req->state = LS_HTTP_FIGURE_OUT_REQ_TARGET_TYPE;
        return LS_ERR_OKAY;
    } else if(likely(P(cursor) == LS_CHAR4_INT('P','O','S','T'))) {
        req->method = LS_HTTP_POST;
        req->cursor = cursor + 4;
        req->state = LS_HTTP_SPACE_AFTER_METHOD;
        return LS_ERR_OKAY;
    } else {
        /* Handle the more uncommon methods */
        switch(P(cursor))
        {
            case LS_CHAR4_INT('P','U','T',' '):
                req->method = LS_HTTP_PUT;
                req->cursor = cursor + 4;
                req->state = LS_HTTP_FIGURE_OUT_REQ_TARGET_TYPE;
                return LS_ERR_OKAY;
            case LS_CHAR4_INT('H','E','A','D'):
                req->method = LS_HTTP_HEAD;
                req->cursor = cursor + 4;
                req->state = LS_HTTP_SPACE_AFTER_METHOD;
                return LS_ERR_OKAY;
            case LS_CHAR4_INT('D','E','L','E'):
                if(*(cursor+4)=='T' && *(cursor+5)=='E') {
                    req->method = LS_HTTP_DELETE;
                    req->cursor = cursor + 6;
                    req->state = LS_HTTP_SPACE_AFTER_METHOD;
                    return LS_ERR_OKAY;
                }
                return LS_ERR_BAD_REQUEST;
            case LS_CHAR4_INT('C','O','N','N'):
                if(P(cursor+4) == LS_CHAR4_INT('E','C','T',' ')) {
                    req->method = LS_HTTP_CONNECT;
                    req->cursor = cursor + 8;
                    req->host_start = cursor;
                    req->state = LS_HTTP_HOST_START;
                    return LS_ERR_OKAY;
                }
                return LS_ERR_BAD_REQUEST;
            case LS_CHAR4_INT('O','P','T','I'):
                if(P(cursor+4) == LS_CHAR4_INT('O','N','S',' ')) {
                    req->method = LS_HTTP_OPTIONS;
                    req->cursor = cursor + 8;
                    req->state = LS_HTTP_FIGURE_OUT_REQ_TARGET_TYPE;
                    return LS_ERR_OKAY;
                }
                return LS_ERR_BAD_REQUEST;
            case LS_CHAR4_INT('T','R','A','C'):
                if(*(cursor+4)=='E') {
                    req->method = LS_HTTP_TRACE;
                    req->cursor = cursor + 5;
                    req->state = LS_HTTP_SPACE_AFTER_METHOD;
                    return LS_ERR_OKAY;
                }
                return LS_ERR_BAD_REQUEST;
            default:
                req->method_start = cursor;
                req->state = LS_HTTP_CUSTOM_HTTP_METHOD;
                return LS_ERR_OKAY;
        }
    }
}

static inline int parse_custom_method(const u_char* cursor, const u_char* end, ls_http_request_t* req)
{
    while(cursor < end && method_chars[*cursor])
        ++cursor;
    if(cursor >= end) {
        req->cursor = cursor;
        return LS_ERR_NEED_MORE_CHARS;
    }
    if(*cursor != ' ') {
        return LS_ERR_BAD_REQUEST;
    }
    req->state = LS_HTTP_FIGURE_OUT_REQ_TARGET_TYPE;
    req->method_end = cursor;
    req->cursor = cursor;
    return LS_ERR_OKAY;
}

static inline int parse_space_after_method(const u_char* cursor, const u_char* end, ls_http_request_t* req)
{
    if(unlikely(*cursor != ' ')) {
        return LS_ERR_BAD_REQUEST;
    }
    if(cursor >= end) {
        req->state = LS_HTTP_SPACE_AFTER_METHOD;
        req->cursor = cursor;
        return LS_ERR_NEED_MORE_CHARS;
    }
    ++cursor;
    req->cursor = cursor;
    req->state = LS_HTTP_FIGURE_OUT_REQ_TARGET_TYPE;
    return LS_ERR_OKAY;
}

static inline int parse_req_target_type(const u_char* cursor, const u_char* end, ls_http_request_t* req)
{
    if(cursor >= end) {
        req->cursor = cursor;
        req->state = LS_HTTP_FIGURE_OUT_REQ_TARGET_TYPE;
        return LS_ERR_NEED_MORE_CHARS;
    }
    if(*cursor == '/') {
        req->path_start = cursor;
        ++cursor;
        req->state = LS_HTTP_AFTER_SLASH_IN_PATH;
        req->cursor = cursor;
        return LS_ERR_OKAY;
    }
    /* OR with 0x20 converts upper to lower and keeps lower, lower */
    /* a <= ch <= z   ->   0 <= ch - 'a' <= 'z' - 'a' */
    /* if ch - 'a' is negative it underflows it will always go larger than 'z' - 'a' anyway */
    if(((*cursor | 0x20) - 'a') <= 'z'-'a') {
        req->state = LS_HTTP_SCHEMA;
        req->schema_start = cursor;
        req->cursor = cursor;
        return LS_ERR_OKAY;
    }
    return LS_ERR_BAD_REQUEST;
}

/* --- schema parsing --- */
static inline int parse_schema(const u_char* cursor, const u_char* end, ls_http_request_t* req)
{
    while(cursor < end && schema_chars[*cursor]) ++cursor;
    if(likely(*cursor == ':')) {
        req->schema_end = cursor++;
        req->cursor = cursor;
        req->state = LS_HTTP_SCHEMA_SLASH;
        return LS_ERR_OKAY;
    }
    if(cursor >= end) {
        req->state = LS_HTTP_SCHEMA;
        req->cursor = cursor;
        return LS_ERR_NEED_MORE_CHARS;
    }
    return LS_ERR_BAD_REQUEST;
}

static inline int parse_schema_slash(const u_char* cursor, const u_char* end, ls_http_request_t* req)
{
    if(likely(*cursor == '/')) {
        ++cursor;
        req->state = LS_HTTP_SCHEMA_SLASH_SLASH;
        req->cursor = cursor;
        return LS_ERR_OKAY;
    }
    if(cursor >= end) {
        req->cursor = cursor;
        req->state = LS_HTTP_SCHEMA_SLASH;
        return LS_ERR_NEED_MORE_CHARS;
    }
    return LS_ERR_BAD_REQUEST;
}

static inline int parse_schema_slash_slash(const u_char* cursor, const u_char* end, ls_http_request_t* req)
{
    if(likely(*cursor == '/')) {
        ++cursor;
        req->state = LS_HTTP_HOST_START;
        req->cursor = cursor;
    }
    if(cursor >= end) {
        req->cursor = cursor;
        req->state = LS_HTTP_SCHEMA_SLASH_SLASH;
        return LS_ERR_NEED_MORE_CHARS;
    }
    return LS_ERR_BAD_REQUEST;
}

/* --- host parsing --- */
static inline int parse_host_start(const u_char* cursor, const u_char* end, ls_http_request_t* req)
{
    /* RFC 3986 Section 3.2.2 states       host = IP-literal / IPv4address / reg-name                */
    /* I let IPv4 be processed as reg_name as reg_name can contain digits and '.'        */
    req->host_start = cursor;
    if(unlikely(*cursor=='[')) {
        req->state = LS_HTTP_HOST_IP_LITERAL;
        req->cursor = ++cursor;
        return LS_ERR_OKAY;
    }
    if(cursor >= end) {
        req->state = LS_HTTP_HOST_START;
        req->cursor = cursor;
        return LS_ERR_NEED_MORE_CHARS;
    }
    req->state = LS_HTTP_HOST_REG_NAME;
    req->cursor = cursor;
    return LS_ERR_OKAY;
}

static inline int parse_host_reg_name(const u_char* cursor, const u_char* end, ls_http_request_t* req)
{
    /* RFC 3986 Section 3.2.2 states       reg-name    = *( unreserved / pct-encoded / sub-delims )  */
    while(cursor < end && reg_name_chars[*cursor]) ++cursor;
    if(cursor >= end) {
        req->state = LS_HTTP_HOST_REG_NAME;
        req->cursor = cursor;
        return LS_ERR_NEED_MORE_CHARS;
    }
    req->host_end = cursor;
    req->state = LS_HTTP_HOST_END;
    req->cursor = cursor;
    return LS_ERR_OKAY;
}

static inline int parse_host_ip_literal(const u_char* cursor, const u_char* end, ls_http_request_t* req)
{
    /* We only really need to support IPv6 literals. IPvFuture doesn't really exist. IPv4 is handled as a regular host_reg_name */
    while(cursor < end && ip_literal_chars[*cursor]) ++cursor;
    if(unlikely(*cursor != ']')) {
        return LS_ERR_BAD_REQUEST;
    }
    if(cursor >= end) {
        req->state = LS_HTTP_HOST_IP_LITERAL;
        req->cursor = cursor;
        return LS_ERR_NEED_MORE_CHARS;
    }
    ++cursor;
    req->host_end = cursor;
    req->cursor = cursor;
    req->state = LS_HTTP_HOST_END;
    return LS_ERR_OKAY;
}

static inline int parse_host_end(const u_char* cursor, const u_char* end, ls_http_request_t* req)
{
    if(*cursor == ':') {
        req->port_start = ++cursor;
        req->cursor = cursor;
        req->state = LS_HTTP_PORT;
        return LS_ERR_OKAY; 
    }

    /* Here we handle all cases of absolute form but a path-rootless (I can't see any use cases and neither does nginx it seems) */
    if(*cursor == '/') {
        /* absolute form && ( (path-abempty && not empty) || path-absolute) */
        req->path_start = cursor++;
        req->cursor = cursor;
        req->state = LS_HTTP_AFTER_SLASH_IN_PATH;
        return LS_ERR_OKAY;
    }
    if(*cursor == '?') {
        /* asbolute form && (path-empty || (path-abempty && empty))  */
        req->query_start = ++cursor;
        req->cursor = cursor;
        req->state = LS_HTTP_HANDLE_QUERY;
        return LS_ERR_OKAY; 
    }
    if(*cursor == ' ') {
        req->state = LS_HTTP_VERSION;
        req->cursor = ++cursor;
        return LS_ERR_OKAY;
    }
    if(cursor >= end) {
        req->cursor = cursor;
        req->state = LS_HTTP_HOST_END;
        return LS_ERR_NEED_MORE_CHARS;
    }
    return LS_ERR_BAD_REQUEST;
}

/* --- port parsing --- */
static inline int parse_port(const u_char* cursor, const u_char* end, ls_http_request_t* req)
{
    while(cursor < end && *cursor >= '0' && *cursor <= '9') ++cursor;
    req->port_end = cursor;

    if(*cursor == '/') {
        req->path_start = cursor++;
        req->cursor = cursor;
        req->state = LS_HTTP_AFTER_SLASH_IN_PATH;
        return LS_ERR_OKAY;
    }
    if(*cursor == '?') {
        req->query_start = ++cursor;
        req->cursor = cursor;
        req->state = LS_HTTP_HANDLE_QUERY;
        return LS_ERR_OKAY;
    }
    if(*cursor == ' ') {
        ++cursor;
        req->cursor = cursor;
        req->state = LS_HTTP_VERSION;
        return LS_ERR_OKAY;
    }
    if(cursor >= end) {
        req->cursor = cursor;
        req->state = LS_HTTP_PORT;
        return LS_ERR_NEED_MORE_CHARS;
    }
    return LS_ERR_BAD_REQUEST;
}

/* --- path parsing --- */
static inline int parse_after_slash_in_path(const u_char* cursor, const u_char* end, ls_http_request_t* req)
{
    while(cursor < end && path_chars[*cursor])
        ++cursor;

    if(cursor >= end) {
        req->cursor = cursor;
        req->state = LS_HTTP_AFTER_SLASH_IN_PATH;
        return LS_ERR_NEED_MORE_CHARS;
    }
    req->path_end = cursor;

    if(*cursor == '?') {
        req->query_start = ++cursor;
        req->cursor = cursor;
        req->state = LS_HTTP_HANDLE_QUERY;
        return LS_ERR_OKAY;
    }
    if(*cursor == ' ') {
        req->version_start = ++cursor;
        req->cursor = cursor;
        req->state = LS_HTTP_VERSION;
        return LS_ERR_OKAY;
    }
    return LS_ERR_BAD_REQUEST;
}

/* --- query parsing --- */
static inline int parse_handle_query(const u_char* cursor, const u_char* end, ls_http_request_t* req)
{
    /* RFC 3986 Section 3.4 states  " query = *( pchar / "/" / "?" ) "  */
    while(cursor < end && (pchar_chars[*cursor] || *cursor=='/' || *cursor=='?')) ++cursor;
    req->query_end = cursor;
    /* Query should be last part of request-target */
    if(*cursor == ' ') {
        req->version_start = ++cursor;
        req->cursor = cursor;
        req->state = LS_HTTP_VERSION;
        return LS_ERR_OKAY;
    }
    if(cursor >= end) {
        req->cursor = cursor;
        req->state = LS_HTTP_HANDLE_QUERY;
        return LS_ERR_NEED_MORE_CHARS; 
    }
    return LS_ERR_BAD_REQUEST; 
}

/* --- HTTP version parsing --- */
static inline int parse_http_version(const u_char* cursor, const u_char* end, ls_http_request_t* req)
{
    /*min now:  "HTTP/x.y\n\n\0" so min of end-cursor = 10 */
    if(end - cursor < 10) {
        return LS_ERR_NEED_MORE_CHARS;
    }

    if(likely(P(cursor) == LS_CHAR4_INT('H','T','T','P'))) {
        /* We have enough clearance in this section from check at start to ensure we don't go out of bounds */
        cursor += 4;
        if(unlikely(*cursor != '/')) { return LS_ERR_BAD_REQUEST; }
        ++cursor;
        req->http_major = *cursor - '0';
        if(req->http_major > 9) { return LS_ERR_BAD_REQUEST; }
        ++cursor;
        if(unlikely(*cursor != '.')) { return LS_ERR_BAD_REQUEST; }
        ++cursor;
        req->http_minor = *cursor - '0';
        if(req->http_minor > 9) { return LS_ERR_BAD_REQUEST; }
        req->state = LS_HTTP_END_OF_REQUEST_LINE;
        req->cursor = ++cursor;
        return LS_ERR_OKAY;
    }
    return LS_ERR_BAD_REQUEST;
}

static inline int parse_end_of_request_line(const u_char* cursor, const u_char* end, ls_http_request_t* req)
{
    if(*cursor == '\r') {
        ++cursor;
        if(likely(*cursor == '\n')) {
            /* In the event that cursor = end after this increment it will be caught when parsing headers and ask for more chars*/
            ++cursor;
            req->state = LS_HTTP_REQ_LINE_DONE;
            req->cursor = cursor;
            return LS_ERR_OKAY;
        }
        if(cursor >= end) {
            req->cursor = cursor - 1;
            return LS_ERR_NEED_MORE_CHARS;
        }
    }
    if(*cursor == '\n') {
        req->state = LS_HTTP_REQ_LINE_DONE;
        req->cursor = ++cursor;
        return LS_ERR_OKAY;
    }
    return LS_ERR_BAD_REQUEST;
}

static int parse_request_line(const u_char* cursor, const u_char* end, ls_http_request_t* req)
{
    int err_code = LS_ERR_OKAY;
    while (1) {
        switch (req->state) {
            case LS_HTTP_METHOD:
                err_code = parse_method(cursor, end, req);
                break;
            case LS_HTTP_CUSTOM_HTTP_METHOD:
                err_code = parse_custom_method(cursor, end, req);
                break;
            case LS_HTTP_SPACE_AFTER_METHOD:
                err_code = parse_space_after_method(cursor, end, req);
                break;
            case LS_HTTP_FIGURE_OUT_REQ_TARGET_TYPE:
                err_code = parse_req_target_type(cursor, end, req);
                break;
            case LS_HTTP_SCHEMA:
                err_code = parse_schema(cursor, end, req);
                break;
            case LS_HTTP_SCHEMA_SLASH:
                err_code = parse_schema_slash(cursor, end, req);
                break;
            case LS_HTTP_SCHEMA_SLASH_SLASH:
                err_code = parse_schema_slash_slash(cursor, end, req);
                break;
            case LS_HTTP_HOST_START:
                err_code = parse_host_start(cursor, end, req);
                break;
            case LS_HTTP_HOST_REG_NAME:
                err_code = parse_host_reg_name(cursor, end, req);
                break;
            case LS_HTTP_HOST_IP_LITERAL:
                err_code = parse_host_ip_literal(cursor, end, req);
                break;
            case LS_HTTP_HOST_END:
                err_code = parse_host_end(cursor, end, req);
                break;
            case LS_HTTP_PORT:
                err_code = parse_port(cursor, end, req);
                break;
            case LS_HTTP_AFTER_SLASH_IN_PATH:
                err_code = parse_after_slash_in_path(cursor, end, req);
                break;
            case LS_HTTP_HANDLE_QUERY:
                err_code = parse_handle_query(cursor, end, req);
                break;
            case LS_HTTP_VERSION:
                err_code = parse_http_version(cursor, end, req);
                break;
            case LS_HTTP_END_OF_REQUEST_LINE:
                err_code = parse_end_of_request_line(cursor, end, req);
                if (req->state == LS_HTTP_REQ_LINE_DONE) {
                    goto done;
                }
                break;
            default:
                return LS_ERR_BAD_REQUEST;
        }

        if (err_code != LS_ERR_OKAY) {
            return err_code;
        }

        cursor = req->cursor;
    }
done:
    return LS_ERR_OKAY;
}

/* === End of funtions for the request line  === */

/* === Start of funtions for the header lines  === */

static ls_trie_node_t* global_header_trie_root = NULL;

void ls_http_parser_init()
{
    global_header_trie_root = ls_http_init_header_trie();
}

/* This function is really bad for cache misses but every time I try to improve it becomes slower anyway */
static inline int parse_header_name(const u_char* cursor, const u_char* end, ls_http_request_t* req)
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
                req->state = LS_HTTP_OWS_BEFORE_VALUE;
                req->header_name_end = cursor;
                req->cursor = ++cursor;
                return LS_ERR_OKAY;
            }
            return LS_ERR_BAD_REQUEST;
        }
        if (req->current_trie_node) {
            req->current_trie_node = req->current_trie_node->children[c];
        }

        ++cursor;
    }

    req->cursor = cursor;
    return LS_ERR_NEED_MORE_CHARS;
}


static inline int parse_ows(const u_char* cursor, const u_char* end, ls_http_request_t* req)
{
    while(cursor < end && (*cursor == ' ' || *cursor == '\t'))
        ++cursor;
    if(cursor >= end)
    {
        req->cursor = cursor;
        return LS_ERR_NEED_MORE_CHARS;
    }
    req->header_value_start = cursor;
    req->state = LS_HTTP_HEADER_VALUE;
    req->cursor = cursor;
    return LS_ERR_OKAY;
}

static inline int parse_header_value(const u_char* cursor, const u_char* end, ls_http_request_t* req)
{
    while(cursor < end) {
        if(*cursor > '\r') {
            ++cursor;
            continue;
        }
        if(*cursor == '\r') {
            req->header_value_end = cursor;
            ++cursor;
            if(*cursor == '\n') {
                req->state = LS_HTTP_STRIP_VALUE_OWS;
                req->cursor = cursor;
                return LS_ERR_OKAY;
            }
            if(cursor >= end) {
                req->cursor = cursor - 1;
                return LS_ERR_NEED_MORE_CHARS;
            }
        }
        else if(*cursor == '\n') {
            req->header_value_end = cursor;
            req->state = LS_HTTP_STRIP_VALUE_OWS;
            req->cursor = cursor;
            return LS_ERR_OKAY;
        }
        else if(*cursor == '\0') {
            return LS_ERR_BAD_REQUEST;
        }
        ++cursor;
    }
    req->cursor = cursor;
    return LS_ERR_NEED_MORE_CHARS;
}

static inline void strip_value_ows(ls_http_request_t* req)
{
    /* End points to the position after the actual end so \r or \n so reduce by 1 */
    const u_char* curr_end = req->header_value_end - 1;
    while(curr_end > req->raw_request && (*curr_end == ' ' || *curr_end == '\t'))
        --curr_end;
    req->header_value_end = curr_end + 1;
    req->state = LS_HTTP_CHECK_END_OF_HEADERS;
}

static inline int check_eohs(const u_char* cursor, const u_char* end, ls_http_request_t* req)
{
    if (*cursor == '\r') {
      ++cursor;
      if (*cursor == '\n') {
        req->state = LS_HTTP_END_OF_HEADERS;
        req->cursor = cursor;
        return LS_ERR_OKAY;
      }
      if (cursor >= end) {
        req->cursor = cursor - 1;
        return LS_ERR_NEED_MORE_CHARS;
      }
    } 
    else if (*cursor == '\n') {
        req->cursor = cursor;
        req->state = LS_HTTP_END_OF_HEADERS;
        return LS_ERR_OKAY;
    }
    return LS_ERR_OKAY;
}

static inline int parse_header_line( const u_char* cursor, const u_char* end, ls_http_request_t* req)
{
    int err_code = LS_ERR_OKAY;
    while (1) {
        switch (req->state) {
            case LS_HTTP_HEADER_NAME:
                err_code = parse_header_name(cursor, end, req);
                break;
            case LS_HTTP_OWS_BEFORE_VALUE:
                err_code = parse_ows(cursor, end, req);
                break;
            case LS_HTTP_HEADER_VALUE:
                err_code = parse_header_value(cursor, end, req);
                break;
            case LS_HTTP_STRIP_VALUE_OWS:
                strip_value_ows(req);
                break;
            case LS_HTTP_CHECK_END_OF_HEADERS:
                err_code = check_eohs(cursor, end, req);
                if(err_code != LS_ERR_OKAY) {
                    return err_code;
                }
                goto done;
            default:
                return LS_ERR_BAD_REQUEST;
        }
        if(err_code != LS_ERR_OKAY) {
            return err_code;
        }
        cursor = req->cursor;
    }
done:
    /* Cursor will be pointing to \n so move on 1 to go to next header line */
    req->cursor = ++cursor;
    return LS_ERR_OKAY;
}

static int parse_header_lines(const u_char* cursor, const u_char* end, ls_http_request_t* req)
{
    /* If user wants to add their own header/field name then we can add it to the trie.
     * this can be done at runtime as well so wouldn't require recompilation  */
    while(1) {
        if(req->state == LS_HTTP_HEADER_NAME) {
            req->header_name_start = cursor;
            req->current_trie_node = global_header_trie_root;
        }
        int err_code = parse_header_line(cursor, end, req);
        cursor = req->cursor; 
        if(err_code != LS_ERR_OKAY) {
            return err_code;
        }
        if(req->state == LS_HTTP_END_OF_HEADERS) {
            return err_code;
        }
        if(!store_header(req)) {
            return LS_ERR_HEADER_STORAGE_FAILURE;
        }
        req->state = LS_HTTP_HEADER_NAME;
    }
}

/* === End of functions for the header lines === */

int ls_http_parse_request(ls_http_request_t* req)
{
    int err_code = LS_ERR_OKAY;
    const u_char* end = req->raw_request + req->request_len;

    if (req->state < LS_HTTP_REQ_LINE_DONE) {
        err_code = parse_request_line(req->cursor, end, req);
        if(err_code != LS_ERR_OKAY) {
            return err_code;
        }
        req->state = LS_HTTP_HEADER_NAME;
    }

    err_code = parse_header_lines(req->cursor, end, req);
    printf("Raw Request: %.*s\n", req->request_len, req->raw_request);
    if(err_code != LS_ERR_OKAY) {
        return err_code;
    }
    req->state = LS_HTTP_DONE;
    return LS_ERR_OKAY;
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
