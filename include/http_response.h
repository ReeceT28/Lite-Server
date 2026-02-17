#pragma once
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "http_parser.h"


#define MAX_NAME_SIZE 32
#define MAX_VALUE_SIZE 128
#define MAX_VERSION_SIZE 32
#define MAX_REASON_SIZE 64
#define MAX_RESPONSE_HEADERS 32
#define MAX_RESPONSE_HEAD_SIZE 4096

/*
 * response_header stores the name and value of a HTTP header for a response.
 * Both should be null terminated to be valid C Strings.
 * The maximum sizes of these arrays are given by MAX_NAME_SIZE and MAX_VALUE_SIZE
 */
typedef struct
{
    char name [MAX_NAME_SIZE];
    char value [MAX_VALUE_SIZE];

} response_header;

/*
 * The http_response struct stores null terminated char arrays (C Strings) to the version and reason.
 * The status code is stored as an int. An array of response_header's and the amount of them currently in the array are stored.
 * A pointer to a file (the body) and its length are stored.
 */
typedef struct
{
    /* Data required to form the status line of a HTTP response */
    char version[MAX_VERSION_SIZE];
    char reason[MAX_REASON_SIZE];
    int status_code;
    /* Data required to form the header section of a HTTP response */
    response_header headers[MAX_RESPONSE_HEADERS];
    size_t header_count;
    char response_head [MAX_RESPONSE_HEAD_SIZE];
    size_t response_head_len;
    /* Data required to form the body section of a HTTP response */
    FILE* body_file;
    size_t body_len;
} http_response;




void build_status_line(http_response* res, http_request* req, int* err_code)
{
    if(memcmp(req->version,"HTTP/1.1",req->version_len) != 0)
    {
        res->status_code = 505;
        snprintf(res->reason, MAX_REASON_SIZE, "HTTP Version Not Supported");
        return;
    }
}

void write_status_line(http_response* res, http_request* req, int* err_code)
{
    res->response_head_len += snprintf(
        res->response_head + res->response_head_len,
        MAX_RESPONSE_HEAD_SIZE - res->response_head_len,
        "%s %d %s\r\n",
        res->version,
        res->status_code,
        res->reason
    );
}

void add_resource(const char* filePath, http_response* res)
{
    FILE* fptr;
    fptr = fopen(filePath, "rb");
    if (fptr == NULL)
    {
        res->status_code = 404;
        snprintf(res->reason, MAX_REASON_SIZE, "Not Found");
        return;
    }
    fseek(fptr, 0L, SEEK_END);
    long int size = ftell(fptr);
    rewind(fptr);

    res->body_file = fptr;
    res->body_len = size;
    char str[MAX_VALUE_SIZE];
    snprintf(str, sizeof str, "%ld", size);
    snprintf(res->headers[res->header_count].name, MAX_NAME_SIZE, "Content-Length");
    snprintf(res->headers[res->header_count++].value, MAX_VALUE_SIZE,"%s", str);

    if(strstr(filePath, ".html"))
    {
        snprintf(res->headers[res->header_count].name, MAX_NAME_SIZE, "Content-Type");
        snprintf(res->headers[res->header_count++].value, MAX_VALUE_SIZE, "text/html");
    }
    else
    {
        fclose(fptr);
        res->body_file = NULL;
        res->status_code = 415;
        snprintf(res->reason, MAX_REASON_SIZE, "Unsupported Media Type");
        return;
    }
}

void handle_get(http_response* res, http_request* req, int* err_code)
{
    char filePath[1024];
    if(memcmp(req->path,"/",1) == 0)
    {
        strcpy(filePath, "./www/index.html");
    }
    else
    {
        snprintf(filePath, sizeof(filePath), "./www%s", req->path);
    }

    add_resource(filePath, res);
}

void write_headers(http_response* res)
{
    for(size_t i = 0; i<res->header_count; i++)
    {
        res->response_head_len += snprintf(
            res->response_head + res->response_head_len,
            MAX_RESPONSE_HEAD_SIZE - res->response_head_len,
            "%s: %s\r\n",
            res->headers[i].name,
            res->headers[i].value
        );
    }
    res->response_head_len += snprintf(
        res->response_head + res->response_head_len,
        MAX_RESPONSE_HEAD_SIZE - res->response_head_len,
        "\r\n"
    );
}

void build_http_response(http_response* res, http_request* req, int* err_code)
{
    // Assume status code of 200 so a valid request first
    res->status_code = 200;
    snprintf(res->reason, MAX_REASON_SIZE,  "OK");
    res->response_head_len = 0;
    snprintf(res->version, MAX_VERSION_SIZE, "%.*s", (int)req->version_len,  req->version);
    res->header_count = 0;

    if(memcmp(req->method,"GET",req->method_len) == 0)
    {
        handle_get(res, req, err_code);
    }
    else
    {
        res->status_code = 405;
        snprintf(res->reason, MAX_REASON_SIZE,  "Method Not Allowed");
    }
    build_status_line(res, req, err_code);
    write_status_line(res, req, err_code);

    write_headers(res);
}



void print_response_info(http_response* res)
{
    printf("=== HTTP Response ===\n");

    printf("%.*s", (int)res->response_head_len, res->response_head);
}
