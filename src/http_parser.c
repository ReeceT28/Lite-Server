#include <stdio.h>
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

// Helper function for scanning the request path
static inline const u_char* parse_path(const u_char* cursor, const u_char* end, http_request* req)
{
    // Hot loop — register-only
    while(cursor < end && path_chars[*cursor])
        ++cursor;

    // Store the end of the path
    req->path_end = cursor;

    return cursor;
}

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



const u_char* parse_request_line_op1(const u_char* cursor, const u_char* end, http_request* req, int* err_code, void** state)
{
    /* Return to previous state if we are calling repeatedly */
    if(state && *state)
        goto *state;
    /**
     * The shortest HTTP request is: "GET / HTTP/1.1\n\n" which is 16 characters long if we allow \n instead of \r\n.
     * The max amount we check for a method is 8 so we can check this condition to ensure the request is valid and all methods can be safely checked. 
     * Without testing I think it is also unlikely that we would read() less than 8 bytes of a request to start with but I am not sure. 
     */
    if (unlikely(cursor + 8 >= end))
    {
        *err_code = LS_ERR_NEED_MORE_CHARS;
        return cursor;
    }


    /* HTTP method */
    /* Fast track for GET and POST which are by far the most common methods */
    if(likely(P(cursor) == LS_CHAR4_INT('G', 'E', 'T', ' ')))
    {
        req->method = LS_HTTP_GET;
        cursor += 4;
        goto figure_out_req_target_type;
    }
    else if(likely(P(cursor) == LS_CHAR4_INT('P', 'O', 'S', 'T')))
    {
        req->method = LS_HTTP_POST;
        cursor += 4;
        goto space_after_method;
    }
    else
    {
        /* Handle the more uncommon methods */
        switch (P(cursor))
        {
        case LS_CHAR4_INT('P', 'U', 'T', ' '):
            req->method = LS_HTTP_PUT;
            cursor += 4; 
            goto figure_out_req_target_type;
        case LS_CHAR4_INT('H', 'E', 'A', 'D'):
            req->method = LS_HTTP_HEAD;
            cursor += 4;
            goto space_after_method;
        case LS_CHAR4_INT('D', 'E', 'L', 'E'):
            if (likely(*(cursor + 4) == 'T' && *(cursor + 5) == 'E'))
            {
                req->method = LS_HTTP_DELETE;
                cursor += 6;
                goto space_after_method;
            }
            *err_code = LS_ERR_BAD_REQUEST;
            return NULL; 
        case LS_CHAR4_INT('C', 'O', 'N', 'N'):
            if (likely(P(cursor + 4) == LS_CHAR4_INT('E', 'C', 'T', ' ')))
            {
                req->method = LS_HTTP_CONNECT;
                cursor += 8;
                req->host_start = cursor;
                goto host_start;
            }
            *err_code = LS_ERR_BAD_REQUEST;
            return NULL; 
        case LS_CHAR4_INT('O', 'P', 'T', 'I'):
            if (likely(P(cursor + 4) == LS_CHAR4_INT('O', 'N', 'S', ' ')))
            {
                req->method = LS_HTTP_OPTIONS;
                cursor += 8;
                goto figure_out_req_target_type;
            }
            *err_code = LS_ERR_BAD_REQUEST;
            return NULL; 
        case LS_CHAR4_INT('T', 'R', 'A', 'C'):
            if (likely(*(cursor + 4) == 'E'))
            {
                req->method = LS_HTTP_TRACE;
                cursor += 5;
                goto space_after_method;
            }
            *err_code = LS_ERR_BAD_REQUEST;
            return NULL; 
        default:
            /* Allow customised http methods */
            req->method_start = cursor;
        custom_http_method:
            while(cursor < end && method_chars[*cursor])
                ++cursor;
            if(cursor >= end)
            {
                *err_code = LS_ERR_NEED_MORE_CHARS;
                *state = &&custom_http_method;
                return cursor;
            }
            if(*cursor != ' ')
            {
                *err_code = LS_ERR_BAD_REQUEST;
                return NULL;
            }
            goto figure_out_req_target_type;
        }
    }
space_after_method:
    /* Skip the space after the method */
    if(cursor >= end)
    {
        *err_code = LS_ERR_NEED_MORE_CHARS;
        *state = &&space_after_method;
        return cursor;
    }
    if(unlikely(*cursor != ' '))
    {
        *err_code = LS_ERR_BAD_REQUEST;
        return NULL;
    }
    ++cursor;
    /* Fall down */
figure_out_req_target_type:
    if(*cursor == '/')
    {
        req->path_start = cursor;
        /* We can still be sure we have clearance from the first check before method */
        ++cursor;
        goto after_slash_in_path; /* Origin form request follows this path */
    } 

    /* OR with 0x20 converts upper to lower and keeps lower, lower */
    /* a <= ch <= z   ->   0 <= ch - 'a' <= 'z' - 'a' */
    /* if ch - 'a' is negative it underflows it will always go larger than 'z' - 'a' anyway */
    if((*cursor | 0x20) - 'a' <= 'z' - 'a'){ goto schema; } /* Absolute form request follows this parth*/
    if(cursor >= end)
    {
        *err_code = LS_ERR_NEED_MORE_CHARS;
        *state = &&figure_out_req_target_type;
        return cursor;
    }
    *err_code = LS_ERR_BAD_REQUEST;
    return NULL;
schema:
    req->schema_start = cursor;
    while(cursor < end && schema_chars[*cursor])
        ++cursor;
    if(likely(*cursor == ':'))
    {
        req->schema_end = cursor;
        ++cursor;
        goto schema_slash;
    }
    if(cursor >= end)
    {
        *err_code = LS_ERR_NEED_MORE_CHARS;
        *state = &&schema;
        return cursor;
    }
    *err_code = LS_ERR_BAD_REQUEST;
    return NULL;
schema_slash:
    if(likely(*cursor == '/'))
    {
        ++cursor;
        goto schema_slash_slash;
    }
    if(cursor >= end)
    {
        *err_code = LS_ERR_NEED_MORE_CHARS;
        *state = &&schema_slash;
        return cursor;
    }
    *err_code = LS_ERR_BAD_REQUEST;
    return NULL;
schema_slash_slash:
    if(likely(*cursor == '/'))
    {
        ++cursor;
        goto host_start;
    }
    if(cursor >= end)
    {
        *err_code = LS_ERR_NEED_MORE_CHARS;
        *state = &&schema_slash_slash;
        return cursor;
    }
    *err_code = LS_ERR_BAD_REQUEST;
    return NULL;
host_start:
    /* RFC 3986 Section 3.2.2 states       host = IP-literal / IPv4address / reg-name                */
    /* I let IPv4 be processed as reg_name as reg_name can contain digits and '.'        */
    req->host_start = cursor;
    if(unlikely(*cursor == '['))
    {
        ++cursor;
        goto host_ip_literal;
    }
    if(cursor >= end)
    {
        *err_code = LS_ERR_NEED_MORE_CHARS;
        *state = &&host_start;
        return cursor;
    }
    /* Fall through to host_reg_name */
host_reg_name:
    /* RFC 3986 Section 3.2.2 states       reg-name    = *( unreserved / pct-encoded / sub-delims )  */
    while(cursor < end && reg_name_chars[*cursor])
        ++cursor;
    if(cursor >= end)
    {
        *err_code = LS_ERR_NEED_MORE_CHARS;
        *state = &&host_reg_name;
        return cursor;
    }
    req->host_end = cursor;
    goto host_end; 
    /* Experiment with moving ip_literal logic before and after host_end */
host_ip_literal:
    /* We only really need to support IPv6 literals. IPvFuture doesn't really exist. IPv4 is handled as a regular host_reg_name */
    while(cursor < end && ip_literal_chars[*cursor])
        ++cursor;
    if(unlikely(*cursor != ']'))
    {
        *err_code = LS_ERR_BAD_REQUEST;
        return NULL;
    }
    if(cursor >= end)
    {
        *err_code = LS_ERR_NEED_MORE_CHARS;
        *state = &&host_ip_literal;
        return cursor;
    }
    ++cursor;
    req->host_end = cursor;
host_end:
    if(*cursor == ':')
    {
        req->port_start = ++cursor;
        goto port;
    } 
    /* Anything beyond this point is basically the path */
    /* Here we handle all cases of absolute form but a path-rootless (I can't see any use cases and neither does nginx it seems) */
    if(*cursor == '/')
    {
        /* absolute form && ( (path-abempty && not empty) || path-absolute) */
        req->path_start = cursor;
        cursor++;
        goto after_slash_in_path;
    }
    if(*cursor == '?')
    {
        /* asbolute form && (path-empty || (path-abempty && empty))  */
        req->query_start = ++cursor;
        goto handle_query;
    }
    if(*cursor == ' ')
    {
        goto http_version;
    }
    if(cursor >= end)
    {
        *err_code = LS_ERR_NEED_MORE_CHARS;
        *state = &&host_end;
        return cursor;
    }
    *err_code = LS_ERR_BAD_REQUEST;
    return NULL;
port:
    while(cursor < end && *cursor >= '0' && *cursor <= '9') 
        ++cursor; 
    req->port_end = cursor;
    /* Anything beyond this point is basically the path */
    /* Here we handle all cases of absolute form but a path-rootless (I can't see any use cases and neither does nginx it seems) */
    if(*cursor == '/')
    {
        /* absolute form && ( (path-abempty && not empty) || path-absolute) */
        req->path_start = cursor;
        cursor++;
        goto after_slash_in_path;
    }
    if(*cursor == '?')
    {
        /* asbolute form && (path-empty || (path-abempty && empty))  */
        req->query_start = ++cursor;
        goto handle_query;
    }
    if(*cursor == ' ')
    {
        ++cursor;
        goto http_version;
    }
    if(cursor >= end)
    {
        *err_code = LS_ERR_NEED_MORE_CHARS;
        *state = &&port;
        return cursor;
    }
after_slash_in_path:
    /* Our path is basically just this now:   *( "/" segment ) */
    /* we either have a ? to start a query or a space to finish path and move onto version*/
    cursor = parse_path(cursor, end, req);  // parse_path is inline, fast, register-only
    if(cursor >= end)
    {
        *err_code = LS_ERR_NEED_MORE_CHARS;
        /* ATTENTION ATTENTION ATTENTION ATTENTION */
        /* ATTENTION ATTENTION ATTENTION ATTENTION */
        /* ATTENTION ATTENTION ATTENTION ATTENTION */
        /* ATTENTION ATTENTION ATTENTION ATTENTION */
        /* ATTENTION ATTENTION ATTENTION ATTENTION */
        /* Swapping this from host_end (wrong) to after_slash_in_path decreases speed by LOTS*/
        *state = &&after_slash_in_path;
        return cursor;
    }
    req->path_end = cursor; 
    if(*cursor == '?')
    {
        /* asbolute form && (path-empty || (path-abempty && empty))  */
        req->query_start = ++cursor;
        goto handle_query;
    }
    if(*cursor == ' ')
    {
        req->version_start = ++cursor;
        goto http_version;
    }
    *err_code = LS_ERR_BAD_REQUEST;
    return NULL;
handle_query:
    /* RFC 3986 Section 3.4 states  " query = *( pchar / "/" / "?" ) "  */
    while(cursor < end && (pchar_chars[*cursor] || *cursor == '/' || *cursor == '?'))
        cursor++;
    req->query_end = cursor;
    /* Query should be last part of request-target */
    if(*cursor == ' ')
    {
        req->version_start = ++cursor;
        goto http_version;
    }
    if (cursor >= end)
    {
        *err_code = LS_ERR_NEED_MORE_CHARS;
        *state = &&handle_query;
        return cursor;
    }
    *err_code = LS_ERR_BAD_REQUEST;
    return NULL;
http_version:
    /*min now:  "HTTP/x.y\n\n\0" so min of end-cursor = 10 */
    if(end-cursor < 10)
    {
        *err_code = LS_ERR_NEED_MORE_CHARS;
        *state = &&http_version;
        return cursor;
    }
    if(likely(P(cursor) == LS_CHAR4_INT('H', 'T', 'T', 'P')))
    {
        /* We have enough clearance in this section from check at start to ensure we don't go out of bounds */
        cursor+=4;
        if(unlikely(*cursor != '/'))
        {
            *err_code = LS_ERR_BAD_REQUEST;
            return NULL;
        }
        ++cursor;
        req->http_major = *cursor - '0';
        if(req->http_major > 9)
        {
            *err_code = LS_ERR_BAD_REQUEST;
            return NULL;
        }
        ++cursor;
        if(unlikely(*cursor != '.'))
        {
            *err_code = LS_ERR_BAD_REQUEST;
            return NULL;
        }
        ++cursor;
        req->http_minor = *cursor - '0';
        if(req->http_minor > 9)
        {
            *err_code = LS_ERR_BAD_REQUEST;
            return NULL;
        }
        goto end_of_request_line;
    }
end_of_request_line:
    /* To get to end_of_request_line we come from http_version, assume minimum size check so have enough clearance for incrementation here*/
    if(*cursor == '\r')
    {
        ++cursor;
        if(likely(*cursor == '\n'))
        {
            ++cursor;
            goto done;
        }
        if (cursor >= end)
        {
            *err_code = LS_ERR_NEED_MORE_CHARS;
            *state = &&end_of_request_line;
            return cursor - 1;
        }
    }
    if(*cursor == '\n')
    {
        /* Send me \r\n if you aren't stupid but I am nice */
        ++cursor;
        goto done;
    }
    *err_code = LS_ERR_BAD_REQUEST;
    return NULL;
done:
    return cursor;
}

/** Need more thorough benchmarking between these to compare which is better, I told chatGPT to rewrite _op1
 * identically but instead of taking in pointer to pointer to label we take in pointer to an integer and we use a switch case
 * statement at the start to determine where we goto. Don't trust chatGPT because it NEVER listens so will have to check if safe if use this one further.
 * At surface level benchmarking it appears _op1 performs minimally better but its very close so hard to say for definite
 * Also there could definitely be differences between these in speed if the request line is sent in multiple chunks.
*/
const u_char* parse_request_line_op2(const u_char* cursor,
                                     const u_char* end,
                                     http_request* req,
                                     int* err_code,
                                     int* state)  // changed from void* to int enum
{
    // Resume point based on enum state
    if(state)
    {
        switch(*state)
        {
            case LS_HTTP_CUSTOM_HTTP_METHOD: goto custom_http_method;
            case LS_HTTP_SPACE_AFTER_METHOD: goto space_after_method;
            case LS_HTTP_FIGURE_OUT_REQ_TARGET_TYPE: goto figure_out_req_target_type;
            case LS_HTTP_SCHEMA: goto schema;
            case LS_HTTP_SCHEMA_SLASH: goto schema_slash;
            case LS_HTTP_SCHEMA_SLASH_SLASH: goto schema_slash_slash;
            case LS_HTTP_HOST_START: goto host_start;
            case LS_HTTP_HOST_REG_NAME: goto host_reg_name;
            case LS_HTTP_HOST_IP_LITERAL: goto host_ip_literal;
            case LS_HTTP_HOST_END: goto host_end;
            case LS_HTTP_PORT: goto port;
            case LS_HTTP_AFTER_SLASH_IN_PATH: goto after_slash_in_path;
            case LS_HTTP_HANDLE_QUERY: goto handle_query;
            case LS_HTTP_VERSION: goto http_version;
            case LS_HTTP_END_OF_REQUEST_LINE: goto end_of_request_line;
            default: break; // LS_HTTP_METHOD or 0 -> start from top
        }
    }

    /**
     * The shortest HTTP request is: "GET / HTTP/1.1\n\n" which is 16 characters long if we allow \n instead of \r\n.
     * The max amount we check for a method is 8 so we can check this condition to ensure the request is valid and all methods can be safely checked. 
     * Without testing I think it is also unlikely that we would read() less than 8 bytes of a request to start with but I am not sure. 
     */
    if (unlikely(cursor + 8 >= end))
    {
        *err_code = LS_ERR_NEED_MORE_CHARS;
        return cursor;
    }


    /* HTTP method */
    /* Fast track for GET and POST which are by far the most common methods */
    if(likely(P(cursor) == LS_CHAR4_INT('G', 'E', 'T', ' ')))
    {
        req->method = LS_HTTP_GET;
        cursor += 4;
        goto figure_out_req_target_type;
    }
    else if(likely(P(cursor) == LS_CHAR4_INT('P', 'O', 'S', 'T')))
    {
        req->method = LS_HTTP_POST;
        cursor += 4;
        goto space_after_method;
    }
    else
    {
        /* Handle the more uncommon methods */
        switch (P(cursor))
        {
        case LS_CHAR4_INT('P', 'U', 'T', ' '):
            req->method = LS_HTTP_PUT;
            cursor += 4; 
            goto figure_out_req_target_type;
        case LS_CHAR4_INT('H', 'E', 'A', 'D'):
            req->method = LS_HTTP_HEAD;
            cursor += 4;
            goto space_after_method;
        case LS_CHAR4_INT('D', 'E', 'L', 'E'):
            if (likely(*(cursor + 4) == 'T' && *(cursor + 5) == 'E'))
            {
                req->method = LS_HTTP_DELETE;
                cursor += 6;
                goto space_after_method;
            }
            *err_code = LS_ERR_BAD_REQUEST;
            return NULL; 
        case LS_CHAR4_INT('C', 'O', 'N', 'N'):
            if (likely(P(cursor + 4) == LS_CHAR4_INT('E', 'C', 'T', ' ')))
            {
                req->method = LS_HTTP_CONNECT;
                cursor += 8;
                req->host_start = cursor;
                goto host_start;
            }
            *err_code = LS_ERR_BAD_REQUEST;
            return NULL; 
        case LS_CHAR4_INT('O', 'P', 'T', 'I'):
            if (likely(P(cursor + 4) == LS_CHAR4_INT('O', 'N', 'S', ' ')))
            {
                req->method = LS_HTTP_OPTIONS;
                cursor += 8;
                goto figure_out_req_target_type;
            }
            *err_code = LS_ERR_BAD_REQUEST;
            return NULL; 
        case LS_CHAR4_INT('T', 'R', 'A', 'C'):
            if (likely(*(cursor + 4) == 'E'))
            {
                req->method = LS_HTTP_TRACE;
                cursor += 5;
                goto space_after_method;
            }
            *err_code = LS_ERR_BAD_REQUEST;
            return NULL; 
        default:
            /* Allow customised http methods */
            req->method_start = cursor;
        custom_http_method:
            while(cursor < end && method_chars[*cursor])
                ++cursor;
            if(cursor >= end)
            {
                *err_code = LS_ERR_NEED_MORE_CHARS;
                *state = LS_HTTP_CUSTOM_HTTP_METHOD;
                return cursor;
            }
            if(*cursor != ' ')
            {
                *err_code = LS_ERR_BAD_REQUEST;
                return NULL;
            }
            goto figure_out_req_target_type;
        }
    }
space_after_method:
    /* Skip the space after the method */
    if(cursor >= end)
    {
        *err_code = LS_ERR_NEED_MORE_CHARS;
        *state = LS_HTTP_SPACE_AFTER_METHOD;
        return cursor;
    }
    if(unlikely(*cursor != ' '))
    {
        *err_code = LS_ERR_BAD_REQUEST;
        return NULL;
    }
    ++cursor;
    /* Fall down */
figure_out_req_target_type:
    if(*cursor == '/')
    {
        req->path_start = cursor;
        /* We can still be sure we have clearance from the first check before method */
        ++cursor;
        goto after_slash_in_path; /* Origin form request follows this path */
    } 

    /* OR with 0x20 converts upper to lower and keeps lower, lower */
    /* a <= ch <= z   ->   0 <= ch - 'a' <= 'z' - 'a' */
    /* if ch - 'a' is negative it underflows it will always go larger than 'z' - 'a' anyway */
    if((*cursor | 0x20) - 'a' <= 'z' - 'a'){ goto schema; } /* Absolute form request follows this parth*/
    if(cursor >= end)
    {
        *err_code = LS_ERR_NEED_MORE_CHARS;
        *state = LS_HTTP_FIGURE_OUT_REQ_TARGET_TYPE;
        return cursor;
    }
    *err_code = LS_ERR_BAD_REQUEST;
    return NULL;
schema:
    req->schema_start = cursor;
    while(cursor < end && schema_chars[*cursor])
        ++cursor;
    if(likely(*cursor == ':'))
    {
        req->schema_end = cursor;
        ++cursor;
        goto schema_slash;
    }
    if(cursor >= end)
    {
        *err_code = LS_ERR_NEED_MORE_CHARS;
        *state = LS_HTTP_SCHEMA;
        return cursor;
    }
    *err_code = LS_ERR_BAD_REQUEST;
    return NULL;
schema_slash:
    if(likely(*cursor == '/'))
    {
        ++cursor;
        goto schema_slash_slash;
    }
    if(cursor >= end)
    {
        *err_code = LS_ERR_NEED_MORE_CHARS;
        *state = LS_HTTP_SCHEMA_SLASH;
        return cursor;
    }
    *err_code = LS_ERR_BAD_REQUEST;
    return NULL;
schema_slash_slash:
    if(likely(*cursor == '/'))
    {
        ++cursor;
        goto host_start;
    }
    if(cursor >= end)
    {
        *err_code = LS_ERR_NEED_MORE_CHARS;
        *state = LS_HTTP_SCHEMA_SLASH_SLASH;
        return cursor;
    }
    *err_code = LS_ERR_BAD_REQUEST;
    return NULL;
host_start:
    /* RFC 3986 Section 3.2.2 states       host = IP-literal / IPv4address / reg-name                */
    /* I let IPv4 be processed as reg_name as reg_name can contain digits and '.'        */
    req->host_start = cursor;
    if(unlikely(*cursor == '['))
    {
        ++cursor;
        goto host_ip_literal;
    }
    if(cursor >= end)
    {
        *err_code = LS_ERR_NEED_MORE_CHARS;
        *state = LS_HTTP_HOST_START;
        return cursor;
    }
    /* Fall through to host_reg_name */
host_reg_name:
    /* RFC 3986 Section 3.2.2 states       reg-name    = *( unreserved / pct-encoded / sub-delims )  */
    while(cursor < end && reg_name_chars[*cursor])
        ++cursor;
    if(cursor >= end)
    {
        *err_code = LS_ERR_NEED_MORE_CHARS;
        *state = LS_HTTP_HOST_REG_NAME;
        return cursor;
    }
    req->host_end = cursor;
    goto host_end; 
    /* Experiment with moving ip_literal logic before and after host_end */
host_ip_literal:
    /* We only really need to support IPv6 literals. IPvFuture doesn't really exist. IPv4 is handled as a regular host_reg_name */
    while(cursor < end && ip_literal_chars[*cursor])
        ++cursor;
    if(unlikely(*cursor != ']'))
    {
        *err_code = LS_ERR_BAD_REQUEST;
        return NULL;
    }
    if(cursor >= end)
    {
        *err_code = LS_ERR_NEED_MORE_CHARS;
        *state = LS_HTTP_HOST_IP_LITERAL;
        return cursor;
    }
    ++cursor;
    req->host_end = cursor;
host_end:
    if(*cursor == ':')
    {
        req->port_start = ++cursor;
        goto port;
    } 
    /* Anything beyond this point is basically the path */
    /* Here we handle all cases of absolute form but a path-rootless (I can't see any use cases and neither does nginx it seems) */
    if(*cursor == '/')
    {
        /* absolute form && ( (path-abempty && not empty) || path-absolute) */
        req->path_start = cursor;
        cursor++;
        goto after_slash_in_path;
    }
    if(*cursor == '?')
    {
        /* asbolute form && (path-empty || (path-abempty && empty))  */
        req->query_start = ++cursor;
        goto handle_query;
    }
    if(*cursor == ' ')
    {
        goto http_version;
    }
    if(cursor >= end)
    {
        *err_code = LS_ERR_NEED_MORE_CHARS;
        *state = LS_HTTP_HOST_END;
        return cursor;
    }
    *err_code = LS_ERR_BAD_REQUEST;
    return NULL;
port:
    while(cursor < end && *cursor >= '0' && *cursor <= '9') 
        ++cursor; 
    req->port_end = cursor;
    /* Anything beyond this point is basically the path */
    /* Here we handle all cases of absolute form but a path-rootless (I can't see any use cases and neither does nginx it seems) */
    if(*cursor == '/')
    {
        /* absolute form && ( (path-abempty && not empty) || path-absolute) */
        req->path_start = cursor;
        cursor++;
        goto after_slash_in_path;
    }
    if(*cursor == '?')
    {
        /* asbolute form && (path-empty || (path-abempty && empty))  */
        req->query_start = ++cursor;
        goto handle_query;
    }
    if(*cursor == ' ')
    {
        ++cursor;
        goto http_version;
    }
    if(cursor >= end)
    {
        *err_code = LS_ERR_NEED_MORE_CHARS;
        *state = LS_HTTP_PORT;
        return cursor;
    }
after_slash_in_path:
    /* Our path is basically just this now:   *( "/" segment ) */
    /* we either have a ? to start a query or a space to finish path and move onto version*/
    cursor = parse_path(cursor, end, req);  // parse_path is inline, fast, register-only
    if(cursor >= end)
    {
        *err_code = LS_ERR_NEED_MORE_CHARS;
        /* ATTENTION ATTENTION ATTENTION ATTENTION */
        /* ATTENTION ATTENTION ATTENTION ATTENTION */
        /* ATTENTION ATTENTION ATTENTION ATTENTION */
        /* ATTENTION ATTENTION ATTENTION ATTENTION */
        /* ATTENTION ATTENTION ATTENTION ATTENTION */
        /* Swapping this from host_end (wrong) to after_slash_in_path decreases speed by LOTS*/
        *state = LS_HTTP_AFTER_SLASH_IN_PATH;
        return cursor;
    }
    req->path_end = cursor; 
    if(*cursor == '?')
    {
        /* asbolute form && (path-empty || (path-abempty && empty))  */
        req->query_start = ++cursor;
        goto handle_query;
    }
    if(*cursor == ' ')
    {
        req->version_start = ++cursor;
        goto http_version;
    }
    *err_code = LS_ERR_BAD_REQUEST;
    return NULL;
handle_query:
    /* RFC 3986 Section 3.4 states  " query = *( pchar / "/" / "?" ) "  */
    while(cursor < end && (pchar_chars[*cursor] || *cursor == '/' || *cursor == '?'))
        cursor++;
    req->query_end = cursor;
    /* Query should be last part of request-target */
    if(*cursor == ' ')
    {
        req->version_start = ++cursor;
        goto http_version;
    }
    if (cursor >= end)
    {
        *err_code = LS_ERR_NEED_MORE_CHARS;
        *state = LS_HTTP_HANDLE_QUERY;
        return cursor;
    }
    *err_code = LS_ERR_BAD_REQUEST;
    return NULL;
http_version:
    /*min now:  "HTTP/x.y\n\n\0" so min of end-cursor = 10 */
    if(end-cursor < 10)
    {
        *err_code = LS_ERR_NEED_MORE_CHARS;
        *state = LS_HTTP_VERSION;
        return cursor;
    }
    if(likely(P(cursor) == LS_CHAR4_INT('H', 'T', 'T', 'P')))
    {
        /* We have enough clearance in this section from check at start to ensure we don't go out of bounds */
        cursor+=4;
        if(unlikely(*cursor != '/'))
        {
            *err_code = LS_ERR_BAD_REQUEST;
            return NULL;
        }
        ++cursor;
        req->http_major = *cursor - '0';
        if(req->http_major > 9)
        {
            *err_code = LS_ERR_BAD_REQUEST;
            return NULL;
        }
        ++cursor;
        if(unlikely(*cursor != '.'))
        {
            *err_code = LS_ERR_BAD_REQUEST;
            return NULL;
        }
        ++cursor;
        req->http_minor = *cursor - '0';
        if(req->http_minor > 9)
        {
            *err_code = LS_ERR_BAD_REQUEST;
            return NULL;
        }
        goto end_of_request_line;
    }
end_of_request_line:
    /* To get to end_of_request_line we come from http_version, assume minimum size check so have enough clearance for incrementation here*/
    if(*cursor == '\r')
    {
        ++cursor;
        if(likely(*cursor == '\n'))
        {
            ++cursor;
            goto done;
        }
        if (cursor >= end)
        {
            *err_code = LS_ERR_NEED_MORE_CHARS;
            *state = LS_HTTP_END_OF_REQUEST_LINE; 
            return cursor - 1;
        }
    }
    if(*cursor == '\n')
    {
        /* Send me \r\n if you aren't stupid but I am nice */
        ++cursor;
        goto done;
    }
    *err_code = LS_ERR_BAD_REQUEST;
    return NULL;
done:
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


