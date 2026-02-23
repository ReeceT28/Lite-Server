#include <memory.h>
#include "server_config.h"
#include "http_parser.h"

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
/* This syntax doesn't work in all C compilers but just use GCC */
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



/** Theory of parser:
 * - Finite state machine parsers sometimes use for loops to iterate the cursor (like nginx). 
 * - They have a switch-case statement which is evaluated at the start of the for loop to determine the current state.
 * - When trying this approach I found it was quite slow (although it is possible this was my own fault).
 * - I decided to use an approach utilising goto to jump to the correct next place in the code and keep track of the state of the parser.
 * - I prefer this for a few reasons:
 * 1. I found the for loop and switch case statement a bit hard to follow as there is a lot of jumping around +- a few hundred lines of code.
 * 2. The catch statement has to be evaluated EVERY time the cursor is iterated - could add quite a lot of overhead.
 * 
 * A lot of people don't like goto but I don't know why. I think its use is justified here and in lower-level/ optimised programming. 
 * I would assume JMP is a very common assembly instruction so why avoid its equivalent in C.
 * end points to a null terminator so is safe to dereference and I take advantage of that at points.
 */
/* Array of function pointers indexed by LS_HTTP_* enum */
parse_func parse_jump_table[] = {
    parse_method,                // LS_HTTP_METHOD
    parse_custom_method,         // LS_HTTP_CUSTOM_HTTP_METHOD
    parse_space_after_method,    // LS_HTTP_SPACE_AFTER_METHOD
    parse_req_target_type,       // LS_HTTP_FIGURE_OUT_REQ_TARGET_TYPE
    parse_schema,                // LS_HTTP_SCHEMA
    parse_schema_slash,          // LS_HTTP_SCHEMA_SLASH
    parse_schema_slash_slash,    // LS_HTTP_SCHEMA_SLASH_SLASH
    parse_host_start,            // LS_HTTP_HOST_START
    parse_host_reg_name,         // LS_HTTP_HOST_REG_NAME
    parse_host_ip_literal,       // LS_HTTP_HOST_IP_LITERAL
    parse_host_end,              // LS_HTTP_HOST_END
    parse_port,                  // LS_HTTP_PORT
    parse_after_slash_in_path,   // LS_HTTP_AFTER_SLASH_IN_PATH
    parse_handle_query,          // LS_HTTP_HANDLE_QUERY
    parse_http_version,          // LS_HTTP_VERSION
    parse_end_of_request_line    // LS_HTTP_END_OF_REQUEST_LINE
};

const u_char* parse_request_line(const u_char* cursor, const u_char* end, http_request* req, int* err_code, int* state)
{
    if (!state) return NULL;

    while (1) {
        if (*state < 0 || *state >= sizeof(parse_jump_table)/sizeof(parse_jump_table[0]))
            return NULL;

        const u_char* next = parse_jump_table[*state](cursor, end, req, err_code, state);
        if (!next) return NULL;

        // If parser tells us to pause (need more chars), return immediately
        if (*err_code == LS_ERR_NEED_MORE_CHARS)
            return next;

        cursor = next;

        // End of request line
        if (*state == LS_HTTP_END_OF_REQUEST_LINE)
            break;
    }

    return cursor;
}

/* === Implementation of parsing functions === */

const u_char* parse_method(const u_char* cursor, const u_char* end, http_request* req, int* err_code, int* state)
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

const u_char* parse_custom_method(const u_char* cursor, const u_char* end, http_request* req, int* err_code, int* state)
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
    return cursor;
}

const u_char* parse_space_after_method(const u_char* cursor, const u_char* end, http_request* req, int* err_code, int* state)
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

const u_char* parse_req_target_type(const u_char* cursor, const u_char* end, http_request* req, int* err_code, int* state)
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
const u_char* parse_schema(const u_char* cursor, const u_char* end, http_request* req, int* err_code, int* state)
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

const u_char* parse_schema_slash(const u_char* cursor, const u_char* end, http_request* req, int* err_code, int* state)
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

const u_char* parse_schema_slash_slash(const u_char* cursor, const u_char* end, http_request* req, int* err_code, int* state)
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
const u_char* parse_host_start(const u_char* cursor, const u_char* end, http_request* req, int* err_code, int* state)
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

const u_char* parse_host_reg_name(const u_char* cursor, const u_char* end, http_request* req, int* err_code, int* state)
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

const u_char* parse_host_ip_literal(const u_char* cursor, const u_char* end, http_request* req, int* err_code, int* state)
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

const u_char* parse_host_end(const u_char* cursor, const u_char* end, http_request* req, int* err_code, int* state)
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
const u_char* parse_port(const u_char* cursor, const u_char* end, http_request* req, int* err_code, int* state)
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
const u_char* parse_after_slash_in_path(const u_char* cursor, const u_char* end, http_request* req, int* err_code, int* state)
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
const u_char* parse_handle_query(const u_char* cursor, const u_char* end, http_request* req, int* err_code, int* state)
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
const u_char* parse_http_version(const u_char* cursor, const u_char* end, http_request* req, int* err_code, int* state)
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

/* --- end-of-line parsing --- */
const u_char* parse_end_of_request_line(const u_char* cursor, const u_char* end, http_request* req, int* err_code, int* state)
{
    if(*cursor == '\r') {
        ++cursor;
        if(likely(*cursor == '\n')) {
            ++cursor;
            *state = LS_HTTP_END_OF_REQUEST_LINE;
            return cursor;
        }
        if(cursor >= end) {
            *err_code = LS_ERR_NEED_MORE_CHARS;
            *state = LS_HTTP_END_OF_REQUEST_LINE;
            return cursor-1;
        }
    }
    if(*cursor == '\n') {
        ++cursor;
        *state = LS_HTTP_END_OF_REQUEST_LINE;
        return cursor;
    }
    *err_code = LS_ERR_BAD_REQUEST;
    return NULL;
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
