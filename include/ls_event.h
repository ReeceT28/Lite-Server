#pragma once
#define LS_READ_CHUNK 8 * 1024
#define LS_MAX_HTTP_SIZE 8 * 1024

typedef struct ls_event_s ls_event_t;

typedef void (*ls_event_handler_ptr)(ls_event_t *event);

typedef struct ls_event_s {
    void* data; 
    ls_event_handler_ptr handler;
} ls_event_t; 

void ls_accept_handler(ls_event_t *ev);
void ls_read_handler(ls_event_t *ev); 
void ls_read_handler_http(ls_event_t *ev); 
