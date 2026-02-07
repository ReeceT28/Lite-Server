#pragma once
#include <stdlib.h>
#include <stdio.h>
#define SEND_BUF_SIZE 4096
#define MAX_REQUEST_HEADERS 32
// RESOLVE_NO_SYMLINKS

/* request_header represents a HTTP header, it is used for requests NOT for responses.
 * It stores pointers to the name and value of a header along with the lengths of their strings.
 * The pointers are to positions in a C string which makes up the original request.
 */
typedef struct
{
    const char* name; /* Name of header */
    size_t name_len; /* Length of name string */
    const char* value; /* Value of header */
    size_t value_len; /* Length of value string */
} request_header;

/* http_request represents a HTTP request.
 * It stores pointers to the positions of the key components of the request and the length of them.
 * The pointers are to positions in a C String which makes up the original request.
 * A pointer to the original request and its length is also stored.
 * The maximum amount of headers is given by MAX_REQUEST_HEADERS.
 */
typedef struct
{
    const char* method;  /* HTTP method e.g. GET */
    size_t method_len; /* Length of method string */
    const char* path; /* Path of the request */
    size_t path_len; /* Length of the path string */
    const char* version; /* The HTTP version */
    size_t version_len; /* Length of the version string */
    request_header headers[MAX_REQUEST_HEADERS]; /* Array of request_headers */
    size_t header_count; /* Number of headers currently in array */
    const char* raw_request; /* Pointer to original raw request */
    size_t request_len; /* Length of the original request */
} http_request;


// Purpose: Parse a token
// Special Parameters: token_start - pointer to pointer of const char which points to start of token, token_len - length of token, delimiter
// Return Values: If success - buf. If fails - nullptr. Also modifies token_start and token_len so is effectively "returning" these
const char* parse_token(const char* buf, const char* buf_end, const char** token_start, size_t* token_len, char delimiter, int* err_code)
{
    if(buf == buf_end){ *err_code = -2; return NULL;}
    *token_start = buf;
    while(buf < buf_end)
    {
        if(*buf == delimiter) break;
        buf++;
    }
    *token_len = buf - *token_start;
    // Currently buf is at the last character of the token (the delimiter) but buf should point to next charater/ byte to be read
    if(buf < buf_end) buf++;

    return buf;
}

const char* skip_ows(const char* buf, const char* buf_end)
{
    while(buf < buf_end && (*buf == ' ' || *buf == '\t')) buf++;
    return buf;
}

// Purpose: Parse what is remaining of a line
const char* parse_line(const char* buf, const char* buf_end, const char** line_start, size_t* line_len,  int* err_code)
{
    if(buf == buf_end){ *err_code = -2; return NULL;}
    *line_start = buf;
    while(buf < buf_end)
    {
        if(*buf == '\r')
        {
            buf++;
            if(buf >= buf_end || *buf != '\n')
            {
                *err_code = -1;
                return NULL;
            }
            *line_len = buf - 1 - *line_start;
            return ++buf;
        }
        buf++;
    }
    *err_code = -2;
    return NULL;
}
// Purpose: Parse the request line
const char* parse_req_line(const char* buf, const char* buf_end, int* err_code, http_request* req)
{
    buf = parse_token(buf, buf_end, &req->method, &req->method_len, ' ', err_code);
    buf = parse_token(buf, buf_end, &req->path, &req->path_len, ' ', err_code);
    buf = parse_line(buf, buf_end, &req->version, &req->version_len, err_code);
    if(*err_code != 0)
    {
        printf("There was an error during parsing the request line\n");
    }
    return buf;
}
const char* parse_headers(const char* buf, const char* buf_end, int* err_code, http_request* req)
{
    while(buf < buf_end)
    {
        // Check for blank line implying end of headers
        if(buf + 1 < buf_end && *buf == '\r' && *(buf+1) == '\n') return buf + 2;

        // Check for too many headers
        if(req->header_count >= sizeof(req->headers) / sizeof(req->headers[0]))
        {
            printf("Error: too many headers");
            *err_code = -3;
            return NULL;
        }

        request_header* header = &req->headers[req->header_count];
        buf = parse_token(buf, buf_end, &header->name, &header->name_len, ':', err_code);
        // Deal with OWS
        buf = skip_ows(buf, buf_end);
        // Get the value
        header->value = buf;
        // parse_line returns pointer to character after \n
        buf = parse_line(buf, buf_end, &header->value, &header->value_len, err_code);
        // Trim white space at the end
        while(*(header->value + header->value_len - 1) == ' ' || *(header->value + header->value_len - 1) == '\t') header->value_len -=1;
        req->header_count++;
    }
    printf("Finished parsing headers");
    return buf;
}

void parse_request(http_request* req, int* err_code)
{
    req->header_count = 0;
    const char* buf = req->raw_request;
    const char* buf_end = req->raw_request + req->request_len;
    buf = parse_req_line(buf, buf_end, err_code, req);
    buf = parse_headers(buf, buf_end, err_code, req);
}

void print_request_info(http_request* req)
{
    size_t i;

    printf("=== HTTP Request ===\n");

    /* Request line */
    printf("Method : %.*s\n",
           (int)req->method_len, req->method);

    printf("Path   : %.*s\n",
           (int)req->path_len, req->path);

    printf("Version: %.*s\n",
           (int)req->version_len, req->version);

    /* Headers */
    printf("\nHeaders (%zu):\n", req->header_count);

    for (i = 0; i < req->header_count; i++) {
        printf("  %.*s: %.*s\n",
               (int)req->headers[i].name_len,
               req->headers[i].name,
               (int)req->headers[i].value_len,
               req->headers[i].value);
    }

    /* Raw buffer info */
    printf("\nRaw buffer length: %zu bytes\n", req->request_len);
}
