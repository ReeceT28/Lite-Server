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



const u_char* parse_request_line_op1(const u_char* cursor, const u_char* end, http_request* req,
                                      const lite_server_config* ls_config, int* err_code)
{
    /**
     * The shortest HTTP request is: "GET / HTTP/1.1\n\n" which is 16 characters long if we allow \n instead of \r\n.
     * Therefore we can make this check, and know it is safe to check all HTTP methods and not discard any valid request.
     * I think it is justified to use the unlikely flag here as most requests to a web server will be valid.
     */
    if (unlikely(cursor + 8 >= end))
    {
        *err_code = LS_ERR_BAD_REQUEST;
        return NULL;
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
            *err_code = LS_ERR_BAD_REQUEST;
            return NULL;
        }
    }
space_after_method:
    /* Skip the space after the method */
    if(likely(*cursor == ' '))
    {
        ++cursor;
    }
    else 
    {
        *err_code = LS_ERR_BAD_REQUEST;
        return NULL;
    }
    /* Fall down to request target type */
figure_out_req_target_type:
    if(*cursor == '/')
    {
        req->path_start = cursor;
        ++cursor;
        goto after_slash_in_path; 
    } /* Origin form request follows this path */

    /* OR with 0x20 converts upper to lower and keeps lower, lower */
    /* a <= ch <= z   ->   0 <= ch - 'a' <= 'z' - 'a' */
    /* if ch - 'a' is negative it underflows it will always go larger than 'z' - 'a' anyway */
    if((*cursor | 0x20) - 'a' <= 'z' - 'a'){ goto schema; } /* Absolute form request follows this parth*/
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
    *err_code = LS_ERR_BAD_REQUEST;
    return NULL;
schema_slash:
    if(likely(*cursor == '/'))
    {
        ++cursor;
        goto schema_slash_slash;
    }
    *err_code = LS_ERR_BAD_REQUEST;
    return NULL;
schema_slash_slash:
    if(likely(*cursor == '/'))
    {
        ++cursor;
        goto host_start;
    }
    *err_code = LS_ERR_BAD_REQUEST;
    return NULL;
// NGINX seems to allow space before the host but I don't like that
host_start:
    /* RFC 3986 Section 3.2.2 states       host = IP-literal / IPv4address / reg-name                */
    req->host_start = cursor;
    if(unlikely(*cursor == '['))
    {
        goto host_ip_literal;
    }
    /* Fall through to host_reg_name */
host_reg_name:
    /* RFC 3986 Section 3.2.2 states       reg-name    = *( unreserved / pct-encoded / sub-delims )  */
    while(cursor < end && reg_name_chars[*cursor])
        ++cursor;
    req->host_end = cursor;
    goto host_end; 
    /* Experiment with moving ip_literal logic before and after host_end */
host_ip_literal:
    /* We only really need to support IPv6 literals. IPvFuture doesn't really exist. IPv4 is handled as a regular host_reg_name */

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
after_slash_in_path:
    /* Our path is basically just this now:   *( "/" segment ) */
    /* we either have a ? to start a query or a space to finish path and move onto version*/
    while(cursor < end && (pchar_chars[*cursor] || *cursor == '/'))
        cursor++;
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
    *err_code = LS_ERR_BAD_REQUEST;
    return NULL;
http_version:
    if(end-cursor < 10)
    {
        *err_code = LS_ERR_BAD_REQUEST;
        return NULL;
    }
    if(likely(P(cursor) == LS_CHAR4_INT('H', 'T', 'T', 'P')))
    {
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
    if(likely(*cursor == '\r'))
    {
        if(likely(*cursor == '\n'))
        {
            goto done;
        }
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