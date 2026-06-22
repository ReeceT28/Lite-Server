#pragma once
#include "ls_http_request.h"
#include "ls_http_response.h"
#define LS_READ_CHUNK 8 * 1024
#define LS_MAX_HTTP_SIZE 8 * 1024

typedef struct ls_event_s ls_event_t;

typedef void (*ls_event_handler_ptr)(ls_event_t *event);

typedef struct ls_event_s {
    void* data; 
    ls_event_handler_ptr handler;
} ls_event_t; 


typedef struct ls_http_ctx_s {
  ls_http_request_t* req;
  ls_http_response_t* res;
} ls_http_ctx_t;

void ls_accept_handler(ls_event_t *ev);
void ls_read_handler(ls_event_t *ev); 
void ls_read_handler_http(ls_event_t *ev); 
void ls_write_handler_http(ls_event_t* ev);

enum {
  LS_HTTP_SEND_OK,
  LS_HTTP_SEND_AGAIN,
  LS_HTTP_SEND_ERR
};