#define _GNU_SOURCE
#include <fcntl.h>
#include <netinet/in.h>
#include <stdio.h>
#include <limits.h>
#include <sys/types.h>
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
#include "ls_connection.h"
#include "ls_server.h"
#include "ls_logging.h"

/**
 * @brief Computes the time remaining until epoll should timeout and the next connection should be timed out 
 * @param worker Pointer to the worker thread which the epoll loop is running on 
 * @return The epoll timeout in milliseconds
 */ 
static int compute_epoll_timeout(ls_worker_t* worker)
{    	
    if (worker->n_connections == 0)
        return -1; 

    uint64_t now = now_ms();
    uint64_t nearest = UINT64_MAX;

    for (size_t i = 0; i < worker->n_connections; ++i) {
        ls_connection_t* conn = worker->connections[i];
        if (conn->expire_at < nearest)
            nearest = conn->expire_at;
    }
    
    if (nearest <= now)
        return 0;

    return (int)(nearest - now);
}

/**
 * @brief Expires any connections on a worker thread that should be timed out 
 * @param worker Pointer to the worker thread which the epoll loop is running on 
 */ 
static void ls_expire_connections(ls_worker_t* worker)
{
    uint64_t now = now_ms();

    for (size_t i = 0; i < worker->n_connections; ++i) {
        ls_connection_t* conn = worker->connections[i];

        if (conn->expire_at <= now) {
            /* Logs disconnect if the server is configured to do so */
            ls_log_disconnect(conn->fd, worker->server->log_cfg);
            epoll_ctl(worker->epfd, EPOLL_CTL_DEL, conn->fd, NULL);
            close(conn->fd);
            /* Free all of the memory associated with the connection */
            ls_http_ctx_t* http_ctx = (ls_http_ctx_t*)conn->protocol_ctx;
            ls_free_pool(http_ctx->pool);
            ls_free_pool(conn->pool);
            /* Swap this now expired connection with the last valid connection */
            size_t idx = conn->index;
            size_t last = worker->n_connections - 1;
            worker->connections[idx] = worker->connections[last];
            worker->connections[idx]->index = idx;
            worker->connections[last] = conn;
            worker->n_connections--;
            /* Decrement the iterator or the element we just swapped to this position would be missed */
            --i;
        }
    }
}

/** 
 * @brief runs the event loop for a worker thread 
 * @param worker Pointer to the worker thread which the epoll loop is running on 
 */ 
void run_event_loop(ls_worker_t* worker) {
    /* Array to store events fetched by epoll_wait */
    struct epoll_event events[1024];

    while (1) {
        /* Compute maximum timeout to wait for events before moving on to expire connections */
        int timeout = compute_epoll_timeout(worker);
        int n = epoll_wait(worker->epfd, events, 1024, timeout);
        if (n == -1) {
            perror("Error on epoll_wait in run_event_loop");
            break;
        }
        /* Handle every event fetched by epoll_wait */
        for (int i = 0; i < n; ++i) {
            ls_event_t* ev = (ls_event_t*)events[i].data.ptr;
            if (ev && ev->handler) {
                ev->handler(ev);
            }
        }
        /* Expires connections that should be timed out */
        ls_expire_connections(worker);
    }
}

int main()
{
    /* Initialise core modules */
    if(ls_http_parser_init() == -1) {
        fprintf(stderr, "ERROR: Failed to initialise the http parsing module when creating the trie for header parsing");
        return -1;
    }
    if(ls_init_alloc() == -1) {
        fprintf(stderr, "ERROR: Failed to initialise the memory allocation module");
        return -1;
    }
    /* Create memory pool and initialise server context */
    ls_mem_pool_t* server_pool =  ls_init_mem_pool(LS_DEFAULT_BLOCK_SIZE);
    if(server_pool == NULL) {
        fprintf(stderr, "ERROR: Failed to create memory pool for the server");
        return -1;
    }
    ls_server_context_t* server_context = ls_palloc(server_pool, sizeof(ls_server_context_t));
    if(server_context == NULL) {
        fprintf(stderr, "ERROR: Failed to allocate memory for the server_context");
        return -1;
    }
    server_context->pool = server_pool;
    /* Configure logging for this server */
    if(ls_log_init(LS_ENABLE_LOGGING | LS_LOG_INFO | LS_LOG_DEBUG | LS_LOG_ERROR | LS_LOG_WARNING, "www/testlog.log", server_context) == -1) {
        fprintf(stderr, "ERROR: Failed to initialise the logging module");
        return -1;
    }
    /* ================================================== FROM THIS POINT ON USE LOGGING TO RECORD INFORMATION E.G. ERRORS  ================================================== */
    /* Create a worker for the server */
    ls_mem_pool_t* worker_pool = ls_init_mem_pool(LS_DEFAULT_BLOCK_SIZE);
    if(worker_pool == NULL) {
        char* msg = "ERROR: Failed to create memory pool for server worker\n";
        ls_log_write(server_context->log_cfg, msg, strlen(msg), LS_LOG_ERROR);
        return -1;
    }
    ls_worker_t* worker = ls_palloc(worker_pool, sizeof(ls_worker_t));
    if(worker == NULL) {
        char* msg = "ERROR: Failed to allocate memory to create server worker\n";
        ls_log_write(server_context->log_cfg, msg, strlen(msg), LS_LOG_ERROR);
        return -1;
    }
    worker->server = server_context;
    worker->pool = worker_pool;
    worker->max_connections = 10000;
    worker->n_connections = 0;
    /* This approach might eat up a lot of memory but I think it should still be negligible for a modern system and I think is a good approach for prioritising fast responses */
    worker->connections = ls_palloc(worker_pool, sizeof(ls_connection_t*) * worker->max_connections);
    if(worker->connections == NULL) {
        char* msg = "ERROR: Failed to allocate memory to initialise connection pool for server worker\n"; 
        ls_log_write(server_context->log_cfg, msg, strlen(msg), LS_LOG_ERROR);
        return -1;
    }
    for(size_t i=0; i<worker->max_connections; ++i){
        worker->connections[i] = ls_palloc(worker_pool, sizeof(ls_connection_t));
        if(worker->connections[i] == NULL){
            char* msg = "ERROR: Failed to allocate memory when creating a connection for the connection pool\n"; 
            ls_log_write(server_context->log_cfg, msg, strlen(msg), LS_LOG_ERROR);
            return -1;
        }
    }

    /* Create the array of listening sockets */
    server_context->lstning_sockets = ls_create_array(server_pool, sizeof(ls_lstning_sock_t), 1);
    if(server_context->lstning_sockets == NULL) {
        char* msg = "ERROR: Failed to create array for server's listening sockets\n"; 
        ls_log_write(server_context->log_cfg, msg, strlen(msg), LS_LOG_ERROR);
        return -1;
    }

    /* Configure the socket to listen on the worker */
    ls_lstning_sock_t* sock = (ls_lstning_sock_t*)ls_array_push(server_context->lstning_sockets);
    if(sock == NULL) {
        char* msg = "ERROR: Failed to push listening socket to the server's array\n"; 
        ls_log_write(server_context->log_cfg, msg, strlen(msg), LS_LOG_ERROR);
        return -1;
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

    /* Actually creates the listening socket and stores fd under sock->fd */
    if(ls_create_lstning_sock(sock) == -1) {
        char* msg = "ERROR: Failed to create listening socket\n"; 
        ls_log_write(server_context->log_cfg, msg, strlen(msg), LS_LOG_ERROR);
        return -1;
    }

    /* Set epoll up for the worker */
    int epfd = epoll_create1(0);
    if (epfd == -1) {
      perror("epoll_create1 in main.c");
      return -1;
    }

    /* Assign epfd to the worker */
    worker->epfd = epfd;

    /* Create event struct used for accepting connections on the listening socket */
    ls_event_t* ev = ls_palloc(worker_pool, sizeof(ls_event_t));
    ev->data = sock;
    ev->handler = ls_accept_handler;

    /* Create the epoll_event and attach the event struct to it so we can call the handler and get the necessary data */
    struct epoll_event ee;
    ee.events = EPOLLIN;
    ee.data.ptr = ev;

    /* Monitor the epoll_event in the epoll loop */
    if (epoll_ctl(epfd, EPOLL_CTL_ADD, sock->fd, &ee) == -1) {
      perror("epoll_ctl in main.c");
      return -1;
    }

    /* Set the root directory where web server's files will be accessed from */
    char abs_root[PATH_MAX];
    realpath("/var/www/my_site/cv-site", abs_root);
    ls_set_root_dir(server_context, abs_root);

    /* Run the event loop */
    run_event_loop(worker);

    /* Clean everything up */
    ls_log_close(server_context->log_cfg);
    ls_free_pool(server_pool);
    ls_free_pool(worker_pool);
    return 0;
}

