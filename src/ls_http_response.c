#define _GNU_SOURCE
#include <stdlib.h>
#include <stdarg.h>
#include <unistd.h>
#include <sys/sendfile.h>
#include <linux/limits.h>
#include <sys/stat.h>
#include <fcntl.h>
#include "ls_http_response.h"
#include "ls_http_parser.h"
#include "ls_server.h"
#include "ls_http_request.h"

int ls_append(ls_http_response_t* res, const char* data, size_t len)
{
    if (res->response_size + len >= MAX_RESPONSE_SIZE)
        return -1;

    memcpy(res->response + res->response_size, data, len);
    res->response_size += len;

    return 0;
}

void ls_write_status_line(ls_http_response_t* res, int http_major, int http_minor, int status, const char* reason)
{
    char buf[128];
    int len = snprintf(buf, sizeof(buf), "HTTP/%d.%d %d %s\r\n", http_major, http_minor, status, reason);

    if (len > 0)
        ls_append(res, buf, (size_t)len);
}

static void ls_handle_get(ls_http_request_t* req, ls_http_response_t* res, ls_server_context_t* server)
{
    if (req->http_major != 1 || req->http_minor != 1) {
        res->status = 505;
        ls_write_status_line(res, 1, 1, 505, "HTTP Version Not Supported");
        ls_append(res, "\r\n", 2);
        return;
    }

    char* path = (char*)req->path_start;
    size_t len = (char*)req->path_end - path;

    const char *root = (const char*)server->root;
    size_t root_len = strlen(root);

    char resolved_path[PATH_MAX];
    char real_path[PATH_MAX];

    // Handle "/"
    if (len == 1 && path[0] == '/') {
        snprintf(resolved_path, PATH_MAX, "%s/index.html", root);
    } else {
        snprintf(resolved_path, PATH_MAX, "%s%.*s", root, (int)len, path);
    }

    // Resolve path
    if (!realpath(resolved_path, real_path)) {
        res->status = 404;
        ls_write_status_line(res, 1, 1, 404, "Not Found");
        ls_append(res, "\r\n", 2);
        return;
    }

    // Enforce root
    if (strncmp(real_path, root, root_len) != 0 ||
        (real_path[root_len] != '/' && real_path[root_len] != '\0')) {
        res->status = 403;
        ls_write_status_line(res, 1, 1, 403, "Forbidden");
        ls_append(res, "\r\n", 2);
        return;
    }

    // Open file
    int fd = open(real_path, O_RDONLY);
    if (fd < 0) {
        res->status = 404;
        ls_write_status_line(res, 1, 1, 404, "Not Found");
        ls_append(res, "\r\n", 2);
        return;
    }

    struct stat st;
    if (fstat(fd, &st) < 0 || !S_ISREG(st.st_mode)) {
        close(fd);
        res->status = 403;
        ls_write_status_line(res, 1, 1, 403, "Forbidden");
        ls_append(res, "\r\n", 2);
        return;
    }

    res->file_fd = fd;
    res->file_size = st.st_size;
    res->status = 200;

    ls_write_status_line(res, 1, 1, 200, "OK");

    const char* content_type = "application/octet-stream";
    char* dot = strrchr(real_path, '.');

    if (dot) {
        if (strcmp(dot, ".html") == 0) content_type = "text/html";
        else if (strcmp(dot, ".css") == 0) content_type = "text/css";
        else if (strcmp(dot, ".js") == 0) content_type = "application/javascript";
        else if (strcmp(dot, ".png") == 0) content_type = "image/png";
        else if (strcmp(dot, ".jpg") == 0 || strcmp(dot, ".jpeg") == 0) content_type = "image/jpeg";
        else if (strcmp(dot, ".txt") == 0) content_type = "text/plain";
    }

    {
        char buf[128];
        int l = snprintf(buf, sizeof(buf), "Content-Type: %s\r\n", content_type);
        if (l > 0) ls_append(res, buf, (size_t)l);
    }

    // Content-Length
    {
        char buf[128];
        int l = snprintf(buf, sizeof(buf), "Content-Length: %ld\r\n", res->file_size);
        if (l > 0) ls_append(res, buf, (size_t)l);
    }

    // Connection header
    // ls_append(res, "Connection: close\r\n", 19);

    // End headers
    ls_append(res, "\r\n", 2);
}

static void ls_handle_post(ls_http_request_t* req, ls_http_response_t* res, ls_server_context_t* server, ls_mem_pool_t* pool)
{
    char* path = (char*)req->path_start;
    size_t len = (char*)req->path_end - path;
    ls_array_t* user_function_arr = server->user_functions;
    ls_resource_function_t* curr = user_function_arr->head;
    size_t idx = 0;
    /* Iterate over all resources associated with a user supplied function and compare with the one in the request */
    while(idx < user_function_arr->n_elmnts)
    {
        curr = &((ls_resource_function_t*)user_function_arr->head)[idx];
        if(strlen(curr->resource) == len && (memcmp(curr->resource, path, len) == 0)) {
            if(curr->user_function(req, res, pool) == -1) {
                /* Won't bother with this for now but generate error 500 internal server error */
                fprintf(stderr, "THIS SHOULD NOT HAPPEN!\n"); 
                return;
            }
            return;
        }
        ++idx; 
    }
    fprintf(stderr, "Failed to find an associated function for POST request\n");
}

ls_http_response_t* ls_build_http_response(ls_mem_pool_t* pool, ls_http_request_t* req, ls_server_context_t* server)
{
    ls_http_response_t* res = ls_palloc(pool, sizeof(ls_http_response_t));
    memset(res, 0, sizeof(ls_http_response_t));
    res->response = ls_palloc(pool, MAX_RESPONSE_SIZE);
    /* Just gonna get POST to call special user defined function for now, don't want to break GET but eventually I will add a check before doing any of the built in handlers
     * which can check to see if a resource is associated with some user defined function and call it if so */ 
    switch(req->method)
    {
        case LS_HTTP_GET:
            ls_handle_get(req, res, server);
            break;
        case LS_HTTP_POST:
            ls_handle_post(req, res, server, pool);
            break;
    }

    return res;
}


ls_http_response_t* ls_build_simple_http_response(ls_mem_pool_t* pool, ls_http_request_t* req, int status, const char* text)
{
    ls_http_response_t* res = ls_palloc(pool, sizeof(ls_http_response_t));
    memset(res, 0, sizeof(ls_http_response_t));
    res->response = ls_palloc(pool, MAX_RESPONSE_SIZE);
    res->file_fd = -1;
    res->status = status;
    ls_write_status_line(res, 1, 1, status, text);
    ls_append(res, "\r\n", 2);
    return res;
}



