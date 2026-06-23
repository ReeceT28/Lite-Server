#include "ls_event.h"
#include <sys/sendfile.h>
#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <sys/epoll.h>
#include <unistd.h>
#include "ls_http_parser_test.h"
#include "ls_utils.h"
#include "ls_http_parser.h"
#include "ls_http_request.h"
#include "ls_connection.h"
#include "ls_mem_pool.h"
#include "ls_server.h"
#include "ls_http_response.h"
#include "ls_logging.h"

/* Accepts connections from clients to a listening socket */
void ls_accept_handler(ls_event_t* ev)
{
    ls_lstning_sock_t* sock = (ls_lstning_sock_t*)ev->data;
    ls_worker_t* worker = sock->worker;

    struct sockaddr_in client_addr;
    socklen_t client_len = sizeof(client_addr);

    int client_fd = accept(sock->fd, (struct sockaddr*)&client_addr, &client_len);
    if (client_fd == -1) {
        perror("accept() in ls_accept_handler");
        return;
    }

    /* Get the status flags */
    int flags = fcntl(client_fd, F_GETFL, 0);
    if (flags == -1) {
        close(client_fd);
        perror("fcntl() in ls_accept_handler");
        return;
    }

    /* Set client socket to be nonblocking */
    flags = flags | O_NONBLOCK;
    if(fcntl(client_fd, F_SETFL, flags) == -1) {
        close(client_fd);
        perror("fcntl() in ls_accept_handler");
        return;
    }

    size_t idx = worker->n_connections;
    if (idx >= worker->max_connections) {
        close(client_fd);
        printf("Worker has reached maximum number of connections \n");
        return;
    }

    /* Get connection from connection pool */
    ls_connection_t* conn = worker->connections[idx];
    memset(conn, 0, sizeof(ls_connection_t));

    /* Create a memory pool to store anything related to the connection */
    ls_mem_pool_t* pool = ls_init_mem_pool(LS_DEFAULT_BLOCK_SIZE);

    if(pool == NULL){
        close(client_fd);
        return;
    }

    /* Setup the connection with the right data */
    conn->index = idx;
    conn->fd = client_fd;
    conn->pool = pool;
    conn->worker = worker;
    conn->expire_at = now_ms() + (5 * 1000);
    conn->closed = 0;

    /* Decide what to attach to epoll next depending on the protocol */
    if(sock->config.type == LS_SOCK_HTTP) {
        conn->read_event.handler = ls_read_handler_http;
        conn->read_event.data = conn;
        ls_http_ctx_t* http_ctx = ls_palloc(pool, sizeof(ls_http_ctx_t));
        if(http_ctx == NULL){
            close(client_fd);
            ls_free_pool(pool);
            printf("Error creating http_ctx in ls_accept_handler\n");
            return;
        }
        http_ctx->pool = ls_init_mem_pool(LS_DEFAULT_BLOCK_SIZE);
        if(http_ctx->pool == NULL) {
            close(client_fd);
            ls_free_pool(pool);
            printf("Error creating pool for http_ctx in ls_accept_handler\n");
        }
        http_ctx->req = ls_create_request(http_ctx->pool);
        http_ctx->res = NULL;
        conn->protocol_ctx = http_ctx;

        http_ctx->req->raw_request = ls_palloc(http_ctx->pool, LS_MAX_HTTP_SIZE);
        http_ctx->req->cursor = http_ctx->req->raw_request;
        http_ctx->res_in_progress = 0;
    }
    else {
        printf("Unsupported protocol type \n");
        close(client_fd);
        ls_free_pool(pool);
        return;
    }

    /* Create epoll_event */
    struct epoll_event ee;
    ee.events = EPOLLIN;
    ee.data.ptr = &conn->read_event;

    /* Attach epoll_event to epoll loop */
    if (epoll_ctl(worker->epfd, EPOLL_CTL_ADD, client_fd, &ee) == -1) {
        perror("epoll_ctl in ls_accept_handler");
        if(sock->config.type == LS_SOCK_HTTP) {
            ls_free_pool(((ls_http_ctx_t*)conn->protocol_ctx)->pool);
        }
        close(client_fd);
        ls_free_pool(pool);
        return;
    };
    /* Increment number of connections on the worker by 1 */
    worker->n_connections++;
}

void ls_read_handler_http(ls_event_t *ev)
{
    ls_connection_t* conn = ev->data;
    if(conn->closed) return;
    ls_http_ctx_t* http_ctx = (ls_http_ctx_t*)conn->protocol_ctx;
    if(http_ctx->res_in_progress) return;
    conn->expire_at = now_ms() + (5 * 1000);
    ls_http_request_t* req = http_ctx->req;

    // calculate how much space is left
    ssize_t remaining = LS_MAX_HTTP_SIZE - req->request_len;
    if (remaining <= 0) {
        printf("request too large\n");
        goto error; // request too large
    }

    size_t to_read = LS_READ_CHUNK < remaining ? LS_READ_CHUNK : remaining;
    u_char* curr_end = req->raw_request + req->request_len;

    ssize_t n = read(conn->fd, curr_end, to_read);

    if (n == -1) {
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            return;
        }
        printf("bad read\n");
        goto error;
    }


    // update buffer length
    req->request_len += n;

    if (req->request_len < LS_MAX_HTTP_SIZE) {
        req->raw_request[req->request_len] = '\0';
    } else {
        printf("request too large\n");
        goto error; // request too large
    }

    // parse whatever we have so far
    int err_code = ls_http_parse_request(req);

    if (err_code != LS_ERR_OKAY) {
        if (err_code == LS_ERR_NEED_MORE_CHARS) {
            // parser wants more data, just return
            // next readable event will call this function again
            return;
        }
        // ls_print_parsed_request(req);
        printf("bad error\n");
        goto error;
    }

    if (req->state == LS_HTTP_DONE) {
        printf("Request for the resource: %.*s\n", (int)(req->path_end - req->path_start), (char *)req->path_start);

        /* Generate a response here */
        ls_http_response_t* res = ls_build_http_response(http_ctx->pool, req, conn->worker->server);
        http_ctx->res = res;

        http_ctx->res_in_progress = 1;

        ls_log_combined(res, conn);

        conn->write_event.data = conn;
        conn->write_event.handler = ls_write_handler_http;
        struct epoll_event ee;
        ee.events = EPOLLOUT;
        ee.data.ptr = &conn->write_event;
        if (epoll_ctl(conn->worker->epfd, EPOLL_CTL_MOD, conn->fd, &ee) == -1) {
          perror("epoll_ctl MOD");
          goto error;
          return;
        }
    }

    // if more data is available, the event loop will call again
    return;

error:
    (void)0; /* Stop weird warning about declaration after label? */

    size_t idx = conn->index;
    ls_worker_t* worker = conn->worker;

    size_t last = worker->n_connections - 1;
    if (idx != last) {
        worker->connections[idx] = worker->connections[last];
        worker->connections[idx]->index = idx;
    }
    worker->n_connections--;

    ls_free_pool(http_ctx->pool);
    ls_close_connection(conn);

    return;
}

static int ls_send_http_response(ls_connection_t* conn, ls_http_response_t* res)
{
    ssize_t n;
    /* 1. Send headers first */
    while (res->response_sent < res->response_size) {
        n = send(conn->fd, res->response + res->response_sent,res->response_size - res->response_sent,0);
        if (n > 0) {
            res->response_sent += n;
            continue;
        }
        /* CHECK THESE */
        if (n == -1) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) return LS_HTTP_SEND_AGAIN;
            perror("send");
            return LS_HTTP_SEND_ERR;
        }
        return LS_HTTP_SEND_ERR;
    }

    /* 2. Then send file */
    while (res->file_fd != -1 && res->file_offset < res->file_size) {
        n = sendfile(conn->fd, res->file_fd, &res->file_offset, res->file_size - res->file_offset);
        if (n > 0) {
            /* sendfile on Linux advances file_offset for us via the pointer */
            if (res->file_offset >= res->file_size) {
                close(res->file_fd);
                res->file_fd = -1;
            }
            continue;
        }
        if (n == 0) {
            close(res->file_fd);
            res->file_fd = -1;
            break;
        }
        if (n == -1) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) return LS_HTTP_SEND_AGAIN;
            perror("sendfile");
            return LS_HTTP_SEND_ERR;
        }
    }

    return LS_HTTP_SEND_OK;
}

void ls_write_handler_http(ls_event_t* ev)
{
    ls_connection_t* conn = ev->data;
    if(conn->closed) return;
    conn->expire_at = now_ms() + (5 * 1000);
    ls_http_ctx_t* http_ctx = (ls_http_ctx_t*)conn->protocol_ctx;
    ls_http_response_t* res = http_ctx->res;

    int rc = ls_send_http_response(conn, res);

    if (rc == LS_HTTP_SEND_OK) {
        /* finished sending: remove EPOLLOUT interest, cleanup response and close connection */
        struct epoll_event ee;
        ee.events = EPOLLIN; /* or 0 if you want to stop monitoring; adjust for keep-alive */
        ee.data.ptr = &conn->read_event; /* or conn, depending on how you store event data */

        /* modify to remove EPOLLOUT; use EPOLL_CTL_MOD if socket already registered */
        if (epoll_ctl(conn->worker->epfd, EPOLL_CTL_MOD, conn->fd, &ee) == -1) {
            printf("HANDLE THIS\n");
            return;
        }
        ls_free_pool(http_ctx->pool);

        http_ctx->pool = ls_init_mem_pool(LS_DEFAULT_BLOCK_SIZE);
        if(http_ctx->pool == NULL) {
            printf("Error creating pool for http_ctx in ls_write_handler_http\n");
        }
        http_ctx->req = ls_create_request(http_ctx->pool);
        http_ctx->res = NULL;
        http_ctx->res_in_progress = 0;

    } else if(rc == LS_HTTP_SEND_ERR) {
        /* fatal error: cleanup */
        printf("FIX THIS\n");
        if (res->file_fd != -1) close(res->file_fd);
        http_ctx->res = NULL;
        ls_free_pool(conn->pool);
        close(conn->fd);
    }
    return;
}
