#define _GNU_SOURCE
#include <fcntl.h>
#include <netinet/in.h>
#include <stdio.h>
#include <limits.h>
#include <sys/socket.h>
#include <netdb.h>
#include <string.h>
#include <pthread.h>
#include <unistd.h>
#include <time.h>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <unistd.h>
#include "ls_http_parser.h"
#include "ls_utils.h"
#include "ls_mem_pool.h"
#include "ls_logging.h"
#include "ls_connection.h"
#include "ls_server.h"

static int compute_epoll_timeout(ls_worker_t* worker)
{
    if (worker->n_connections == 0)
        return -1; // no timers pending

    uint64_t now = now_ms();
    uint64_t nearest = UINT64_MAX;

    for (size_t i = 0; i < worker->n_connections; i++) {
        ls_connection_t* conn = worker->connections[i];

        if (conn->expire_at < nearest)
            nearest = conn->expire_at;
    }

    if (nearest <= now)
        return 0;

    return (int)(nearest - now);
}

static void ls_expire_connections(ls_worker_t* worker)
{
    uint64_t now = now_ms();

    for (size_t i = 0; i < worker->n_connections; i++) {
        ls_connection_t* conn = worker->connections[i];

        if (conn->expire_at <= now) {
            printf("closing connection\n");

            epoll_ctl(worker->epfd, EPOLL_CTL_DEL, conn->fd, NULL);

            ls_log_disconnect(conn->fd, worker->server->log_cfg);

            conn->closed = 1;

            close(conn->fd);

            // if you free pools here, do it here (and set conn->pool=NULL)
            if (conn->pool) {
              ls_free_pool(conn->pool);
              conn->pool = NULL;
            }
            conn->protocol_ctx = NULL;
            conn->fd = -1;

            worker->connections[i] =
                worker->connections[worker->n_connections - 1];
            worker->n_connections--;
            i--;
        }
    }
}

void run_event_loop(ls_worker_t* worker) {
    struct epoll_event events[1024];

    while (1) {
        int timeout = compute_epoll_timeout(worker);
        int n = epoll_wait(worker->epfd, events, 1024, timeout);
        if (n == -1) {
            perror("epoll_wait");
            break;
        }

        for (int i = 0; i < n; i++) {
            ls_event_t *ev = (ls_event_t *)events[i].data.ptr;
            if (ev && ev->handler) {
                ev->handler(ev);
            }
        }
        ls_expire_connections(worker);
    }
}

int main()
{
    ls_http_parser_init();
    ls_init_alloc();

    // parser_run_benchmark(1000000);
    // return 0;

    /* Create memory pool and initialise server context */
    ls_mem_pool_t* server_pool =  ls_init_mem_pool(LS_DEFAULT_BLOCK_SIZE);
    ls_server_context_t* server_context = ls_palloc(server_pool, sizeof(ls_server_context_t));
    /* Add listening socket */
    server_context->pool = server_pool;
    ls_log_init(LS_ENABLE_LOGGING | LS_LOG_FORMAT_COMBINED | LS_LOG_EVENT_DISCONNECT, "www/testlog.log", server_context);
    server_context->lstning_sockets = ls_create_array(server_pool, sizeof(ls_lstning_sock_t), 1);

    ls_mem_pool_t* worker_pool = ls_init_mem_pool(LS_DEFAULT_BLOCK_SIZE);
    ls_worker_t* worker = ls_palloc(worker_pool, sizeof(ls_worker_t));
    worker->server = server_context;
    worker->pool = worker_pool;
    worker->max_connections = 10000;
    worker->n_connections = 0;
    worker->connections = ls_palloc(worker_pool, sizeof(ls_connection_t*) * worker->max_connections);
    for(size_t i=0; i<worker->max_connections; i++){
        worker->connections[i] = ls_palloc(worker_pool, sizeof(ls_connection_t));
        if(worker->connections[i] == NULL){
            return -1;
        }
    }


    ls_lstning_sock_t* sock = (ls_lstning_sock_t*)ls_array_push(server_context->lstning_sockets);
    if(sock == NULL) {
        printf("Error pushing onto array for listening sockets \n");
    }
    sock->worker = worker;
    sock->config = (ls_socket_conf_t){
        .family = AF_INET,
        .socktype = SOCK_STREAM,
        .host = NULL,
        .port = 8080,
        .backlog = SOMAXCONN,
        .type = LS_SOCK_HTTP
    };
    printf("Creating socket: family=%d, type=%d\n", sock->config.family, sock->config.socktype);
    if(ls_create_lstning_sock(sock) == -1) {
        printf("Error creating listening socket \n");
        return -1;
    }

    /* Get stuff ready for epoll loop */
    int epfd = epoll_create1(0);
    if (epfd == -1) {
      perror("epoll_create1");
      return -1;
    }
    /* Assign epfd to server context */
    worker->epfd = epfd;
    /* Create accept handler for */
    ls_event_t* ev = ls_palloc(worker_pool, sizeof(ls_event_t));
    ev->data = sock;
    ev->handler = ls_accept_handler;

    struct epoll_event ee;
    ee.events = EPOLLIN;
    ee.data.ptr = ev;

    if (epoll_ctl(epfd, EPOLL_CTL_ADD, sock->fd, &ee) == -1) {
      perror("epoll_ctl");
      return -1;
    }

    char abs_root[PATH_MAX];
    realpath("/var/www/test", abs_root);
    ls_set_root_dir(server_context, abs_root);

    printf("Server root: %p\n", server_context->root);

    run_event_loop(worker);

    ls_log_close(server_context->log_cfg);
    ls_free_pool(server_pool);
    ls_free_pool(worker_pool);
    return 0;
}


/*
 * NEXT S- TheTEPS:
 * Prevent Directory Traversal by using more secure functions like openat2(),
 * can take inspiration from
 * https://github.com/google/safeopen/blob/66b54d5181c6fee7ceb20782daca922b24f62819/safeopen_linux.go
 * Handle multiple clients, look into Having Open connections
 * Support more methods like POST
 * Non blocking accepting of connections
 * Making public
 * */
