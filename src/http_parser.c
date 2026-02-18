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

static inline unsigned int P(unsigned char *p)
{
#if (LS_HAVE_EFFICIENT_UNALIGNED_ACCESS)
        return (*(uint32_t *)(p));
#else
	return ((unsigned int)((p)[0]) | ((unsigned int)((p)[1]) << 8) |
		    ((unsigned int)((p)[2]) << 16) | ((unsigned int)((p)[3]) << 24));
#endif
}

const u_char* parse_request_line_op1(const u_char* cursor, const u_char* end, http_request* req,
                                      const lite_server_config* ls_config, int* err_code)
{
    req->method_start = cursor;
    const u_char* p = cursor;

    if (unlikely(p + 8 >= end))  // Need at least 4–8 bytes to safely read method
        return cursor;

    if(likely(P(p) == LS_CHAR4_INT('G', 'E', 'T', ' ')))
    {
        req->method = LS_HTTP_GET;
        p += 4;
    }
    else if(likely(P(p) == LS_CHAR4_INT('P', 'O', 'S', 'T')))
    {
        req->method = LS_HTTP_POST;
        p += 4;
    }
    else
    {
        // Fast detection of common HTTP methods using 4-byte integer reads
        switch (P(p))
        {
        case LS_CHAR4_INT('P', 'U', 'T', ' '):
            req->method = LS_HTTP_PUT;
            p += 3; // "PUT" is 3 chars
            break;
        case LS_CHAR4_INT('H', 'E', 'A', 'D'):
            req->method = LS_HTTP_HEAD;
            p += 4;
            break;
        case LS_CHAR4_INT('D', 'E', 'L', 'E'):
            if (likely(*(p + 4) == 'T' && *(p + 5) == 'E'))
            {
                req->method = LS_HTTP_DELETE;
                p += 6;
            }
            break;
        case LS_CHAR4_INT('C', 'O', 'N', 'N'):
            if (likely(P(p + 4) == LS_CHAR4_INT('E', 'C', 'T', ' ')))
            {
                req->method = LS_HTTP_CONNECT;
                p += 7;
            }
            break;
        case LS_CHAR4_INT('O', 'P', 'T', 'I'):
            if (likely(P(p + 4) == LS_CHAR4_INT('O', 'N', 'S', ' ')))
            {
                req->method = LS_HTTP_OPTIONS;
                p += 7;
            }
            break;
        case LS_CHAR4_INT('T', 'R', 'A', 'C'):
            if (likely(*(p + 4) == 'E'))
            {
                req->method = LS_HTTP_TRACE;
                p += 5;
            }
            break;
        default:
            *err_code = -1;
            return NULL;
        }
    }

    // Skip the space after the method
    if (likely(*p == ' '))
        ++p;

    return p;
}


const u_char* parse_request_line_op2(const u_char* cursor, const u_char* end, http_request* req,
    const lite_server_config* ls_config, int* err_code)
{
    req->method_start = cursor;
    const u_char* start = cursor;

    while (cursor < end && *cursor != ' ')
        ++cursor;

    switch (cursor - start)
    {
    case 3:
        if (memcmp(start, "GET", 3) == 0) {
            req->method = LS_HTTP_GET;
            break;
        }
        if (memcmp(start, "PUT", 3) == 0) {
            req->method = LS_HTTP_PUT;
            break;
        }
        break;

    case 4:
        if (memcmp(start, "POST", 4) == 0) {
            req->method = LS_HTTP_POST;
            break;
        }
        if (memcmp(start, "HEAD", 4) == 0) {
            req->method = LS_HTTP_HEAD;
            break;
        }
        break;

    case 5:
        if (memcmp(start, "TRACE", 5) == 0) {
            req->method = LS_HTTP_TRACE;
            break;
        }
        break;

    case 6:
        if (memcmp(start, "DELETE", 6) == 0) {
            req->method = LS_HTTP_DELETE;
            break;
        }
        break;

    case 7:
        if (memcmp(start, "CONNECT", 7) == 0) {
            req->method = LS_HTTP_CONNECT;
            break;
        }
        if (memcmp(start, "OPTIONS", 7) == 0) {
            req->method = LS_HTTP_OPTIONS;
            break;
        }
        break;
    }
    return cursor;
}

const u_char* parse_request_line_op3(const u_char* cursor, const u_char* end, http_request* req,
                                      const lite_server_config* ls_config, int* err_code)
{
    req->method_start = cursor;
    const u_char* p = cursor;

    if (unlikely(p + 8 >= end))  // Need at least 4–8 bytes to safely read method
        return cursor;
    // Fast detection of common HTTP methods using 4-byte integer reads
    switch (P(p))
    {
    case LS_CHAR4_INT('G', 'E', 'T', ' '):
        req->method = LS_HTTP_GET;
        p += 4;
        break;
    case LS_CHAR4_INT('P', 'O', 'S', 'T'):
        req->method = LS_HTTP_POST;
        p += 4;
        break;
    case LS_CHAR4_INT('P', 'U', 'T', ' '):
        req->method = LS_HTTP_PUT;
        p += 3; // "PUT" is 3 chars
        break;
    case LS_CHAR4_INT('H', 'E', 'A', 'D'):
        req->method = LS_HTTP_HEAD;
        p += 4;
        break;
    case LS_CHAR4_INT('D', 'E', 'L', 'E'):
        if (likely(*(p + 4) == 'T' && *(p + 5) == 'E'))
        {
            req->method = LS_HTTP_DELETE;
            p += 6;
        }
        break;
    case LS_CHAR4_INT('C', 'O', 'N', 'N'):
        if (likely(P(p + 4) == LS_CHAR4_INT('E', 'C', 'T', ' ')))
        {
            req->method = LS_HTTP_CONNECT;
            p += 7;
        }
        break;
    case LS_CHAR4_INT('O', 'P', 'T', 'I'):
        if (likely(P(p + 4) == LS_CHAR4_INT('O', 'N', 'S', ' ')))
        {
            req->method = LS_HTTP_OPTIONS;
            p += 7;
        }
        break;
    case LS_CHAR4_INT('T', 'R', 'A', 'C'):
        if (likely(*(p + 4) == 'E'))
        {
            req->method = LS_HTTP_TRACE;
            p += 5;
        }
        break;
    default:
        *err_code = -1;
        return NULL;
    }

    // Skip the space after the method
    if (likely(*p == ' '))
        ++p;

    return p;
}

// Research of URI parsing:
/* RFC 9112 Section 3.2 states       " request-target = origin-form / absolute-form / authority-form / asterisk-form "          */
/* RFC 9112 Section 3.2.1 states     " origin-form = absolute-path [ "?" query ]"                                               */
/* RFC 9110 Section 4.1 states       " absolute-path = 1*( "/" segment )"                                                       */
/* RFC 3986 Section 3.3 states       " segment = *pchar "                                                                       */
/* RFC 3986 Section 3.3 states       " pchar = unreserved / pct-encoded / sub-delims / ":" / "@" "                              */
/* RFC 3986 Section 2.3 states       " unreserved = ALPHA / DIGIT / "-" / "." / "_" / "~" "                                     */
/* RFC 3986 Section 2.1 states       " pct-encoded = "%" HEXDIG HEXDIG "                                                        */
/* RFC 3986 Section 2.1 states       " sub-delims = "!" / "$" / "&" / "'" / "(" / ")" / "*" / "+" / "," / ";" / "=" "           */
/* Okay so we can figure out what an absolute-path is now, next we need the query of an origin-form                             */
/* RFC 3986 Section 3.4 states       " query = *( pchar / "/" / "?" ) "                                                         */
/* We already know what pchar is, we haven't actually added any new characters here if we are considering origin-form           */
/* Now we just need absolute-form / authority-form / asterisk-form                                                              */
/* RFC 9112 Section 3.2.2 states     " absolute-form = absolute-URI "                                                           */
/* RFC 3986 Section 4.3 states       " absolute-URI = scheme ":" hier-part [ "?" query ] "                                      */
/* RFC 3986 Section 3.1 states       " scheme = ALPHA *( ALPHA / DIGIT / "+" / "-" / "." ) "                                    */
/* RFC 3986 Section 3.0 states       " hier-part = "//" authority path-abempty / path-absolute / path-rootless / path-empty "   */
/* Now we need to figure out what all these combinations of hier-part can be for absolyte URI                                   */
/* RFC 3986 Section 3.2 states       " authority = [ userinfo "@" ] host [ ":" port ]"*/
/* RFC 3986 Section 3.2.1 states     " userinfo = *( unreserved / pct-encoded / sub-delims / ":" ) "                            */
/* userinfo adds no new characters as it is just pchar without "@" */
/* RFC 3986 Section 3.2.2 states     " host = IP-literal / IPv4address / reg-name "                                             */

/* MAYBE JUST ALLOW ANY unreserved, resered of pct-encoded*/
/* Maybe just allow any VCHAR */
/* RFC 3986 Section 3.3 states       "path-abempty" */