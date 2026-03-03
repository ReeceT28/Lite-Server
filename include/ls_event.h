#pragma once

typedef struct ls_event_s ls_event_t;

typedef void (*ls_event_handler_ptr)(ls_event_t *event);

typedef struct ls_event_s {
    void* data; 
    ls_event_handler_ptr handler;
} ls_event_t; 