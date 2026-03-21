#include <memory.h>
#include "ls_http_request.h"
#include "ls_http_parser.h"
#include "ls_mem_pool.h"

int store_header(ls_http_request_t *req)
{
    if (req->headers->n_elmnts >= req->header_capacity) {
        return 0;
    }
    ls_header_t *h = ls_array_push(req->headers);
    h->name_start  = req->header_name_start;
    h->name_end    = req->header_name_end;
    h->value_start = req->header_value_start;
    h->value_end   = req->header_value_end;
    h->header_id = req->header_id;
    return 1;
}

ls_http_request_t* ls_create_request(ls_mem_pool_t* pool)
{
    ls_http_request_t* r = ls_palloc(pool, sizeof(*r));
    if (!r) return NULL;

    memset(r, 0, sizeof(*r));

    r->headers = ls_create_array(pool, sizeof(ls_header_t), LS_MAX_REQUEST_HEADERS);
    if (!r->headers) return NULL;

    r->header_capacity = LS_MAX_REQUEST_HEADERS;
    r->state = LS_HTTP_METHOD;
    return r;
}