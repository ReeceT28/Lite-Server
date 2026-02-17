#include "server_config.h"
#include "http_parser.h"

const char* parse_token(const char* start, const char* end, const char** token_start, size_t* token_len, const char delimiter,
    int (*is_valid)(unsigned char), int* err_code)
{
    *token_start = start;
    while(start < end && *start != delimiter)
    {
        if(!is_valid((unsigned char)*start))
        {
            *err_code = -4;
            return NULL;
        }
        ++start;
    }
    if(UNLIKELY(start == *token_start))
    {
        *err_code = ERR_EMPTY_TOKEN_NAME;
        return NULL;
    }
    if(UNLIKELY(start >= end))
    {
        *err_code = ERR_MISSING_TOKEN_DELIMITER;
        return NULL;
    }
    *token_len = start - *token_start;
    return start + 1; 
}

const char* parse_token_fast(const char* start, const char* end, const char** token_start, size_t* token_len, const char delimiter)
{
    *token_start = start;
    while(start < end && *start != delimiter)
        ++start;
    *token_len = start - *token_start;
    return start >= end ? NULL : start + 1; 
}

const char* parse_request_line(const char* cursor, const char* end, http_request* req, const lite_server_config* ls_config, int* err_code)
{
    if(ls_config->flags & LS_ALLOW_LEADING_CRLF)
    {
        cursor = skip_leading_crlf(cursor, end, err_code);
    }
    if(ls_config->flags & LS_ENFORCE_GRAMMAR){
        cursor = parse_token(cursor, end, &req->method, &req->method_len, ' ', &is_tchar ,err_code);
    } else {
        cursor = parse_token_fast(cursor, end, &req->method, &req->method_len, ' ');
    }

    if(cursor == NULL) return NULL;
    cursor = skip_ows(cursor, end);
    if(cursor >= end) return NULL;
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

    if(ls_config->flags & LS_ENFORCE_GRAMMAR){
        cursor = parse_token(cursor, end, &req->path, &req->path_len, ' ', &is_tchar ,err_code);
    } else {
        cursor = parse_token_fast(cursor, end, &req->path, &req->path_len, ' ');
    }

}


void parse_request(http_request* req, int* err_code, const lite_server_config* lt_config)
{
    req->header_count = 0;
    parse_request_line(req, err_code, lt_config, err_code);
}