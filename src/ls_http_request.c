#include "ls_http_request.h"
int store_header(ls_http_request_t *req)
{
    if (req->header_count >= req->header_capacity) {
        return 0;
    }
    ls_header_t *h = &req->headers[req->header_count++];
    h->name_start  = req->header_name_start;
    h->name_end    = req->header_name_end;
    h->value_start = req->header_value_start;
    h->value_end   = req->header_value_end;
    h->header_id = req->header_id;
    return 1;
}