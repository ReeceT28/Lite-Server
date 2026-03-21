#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <sys/epoll.h>
#include <unistd.h>
#include "ls_http_parser_test.h"
#include "ls_http_parser.h"
#include "ls_http_request.h"
#include "ls_event.h"
#include "ls_connection.h"
#include "ls_mem_pool.h"
#include "ls_server.h"


void ls_accept_handler(ls_event_t* ev)
{
    printf("Attempting to accept connection \n");
    ls_lstning_sock_t* sock = (ls_lstning_sock_t*)ev->data;
    ls_server_context_t* server = sock->server;

    int client_fd = accept(sock->fd, NULL, NULL);
    if (client_fd == -1) {
        perror("accept");
        return;
    }

    int flags = fcntl(client_fd, F_GETFL, 0);
    if (flags == -1) {
        close(client_fd);
        perror("fcntl");
        return;
    }
    flags = flags | O_NONBLOCK;
    if(fcntl(client_fd, F_SETFL, flags) == -1) {
        close(client_fd);
        perror("fcntl");
        return;
    }

    ls_mem_pool_t* pool = ls_init_mem_pool(LS_DEFAULT_BLOCK_SIZE);

    // create connection + event for client
    ls_connection_t* conn = ls_palloc(pool, sizeof(ls_connection_t));
    if(conn == NULL) { 
        ls_free_pool((pool));
        close(client_fd);
        return;
    }

    conn->fd = client_fd;
    conn->server = server;
    conn->pool = pool;
    conn->read_event.data = conn;
    if(sock->config.type == LS_SOCK_HTTP) {
        conn->read_event.handler = ls_read_handler_http;
        conn->protocol_ctx = ls_create_request(pool);
        ((ls_http_request_t*)conn->protocol_ctx)->raw_request = ls_palloc(conn->pool, LS_MAX_HTTP_SIZE);
    }
    else {
        printf("Unsupported protocol type \n");
        close(client_fd);
        ls_free_pool(pool);
        return;
    }

    struct epoll_event ee;
    ee.events = EPOLLIN;
    ee.data.ptr = &conn->read_event;

    if (epoll_ctl(server->epfd, EPOLL_CTL_ADD, client_fd, &ee) == -1) {
      perror("epoll_ctl");
      close(client_fd);
      ls_free_pool(pool);
      return;
    }
}

void ls_read_handler_http(ls_event_t *ev) 
{
    ls_connection_t* conn = ev->data;
    ls_http_request_t* req = conn->protocol_ctx;

    // calculate how much space is left
    size_t remaining = LS_MAX_HTTP_SIZE - req->request_len;
    if (remaining <= 0) {
        goto error; // request too large
    }

    size_t to_read = LS_READ_CHUNK < remaining ? LS_READ_CHUNK : remaining;
    u_char* curr_end = req->raw_request + req->request_len;

    ssize_t n = read(conn->fd, curr_end, to_read);

    if (n == -1) {
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            return;
        }
        goto error;
    }

    if (n == 0) {
        // client closed connection
        goto error;
    }

    // update buffer length
    req->request_len += n;

    if (req->request_len < LS_MAX_HTTP_SIZE) {
        req->raw_request[req->request_len] = '\0';
    } else {
        goto error; // request too large
    }

    // parse whatever we have so far
    int err_code = LS_ERR_OKAY;
    const u_char* cursor = curr_end;
    const u_char* end = curr_end + n;

    const u_char* new_cursor =
        ls_http_parse_request(cursor, end, req, &err_code, &req->state);

    if (!new_cursor) {
        goto error;
    }

    if (err_code == LS_ERR_NEED_MORE_CHARS) {
        // parser wants more data, just return
        // next readable event will call this function again
        return;
    }

    if (req->state == LS_HTTP_DONE) {
        ls_print_parsed_request(req);
        close(conn->fd);
        ls_free_pool(conn->pool);
        return;
    }

    // if more data is available immediately, the event loop will call again
    return;

error:
    printf("Error parsing \n");
    close(conn->fd);
    ls_free_pool(conn->pool);
}