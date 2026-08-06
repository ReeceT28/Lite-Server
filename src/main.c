#define _GNU_SOURCE
#include <stdint.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <stdio.h>
#include <limits.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/resource.h>
#include <netdb.h>
#include <string.h>
#include <pthread.h>
#include <unistd.h>
#include <time.h>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <unistd.h>
#include <errno.h>
#include <signal.h>
#include "ls_http_parser.h"
#include "ls_utils.h"
#include "ls_mem_pool.h"
#include "ls_connection.h"
#include "ls_server.h"
#include "ls_logging.h"

/**
 * @brief Computes the time remaining until epoll should timeout and the next connection should be timed out
 * @param worker Pointer to the worker thread which the epoll loop is running on
 * @return On success the epoll timeout in milliseconds (or -1 for infinity), On failure -67
 */
static int compute_epoll_timeout(ls_worker_t* worker)
{
    if (worker->n_connections == 0)
        return -1;

    uint64_t now = now_ms();
    if(now == UINT64_MAX) {
        return -67;
    }
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
 * @return On success 0, On failure -1
 */
static int ls_expire_connections(ls_worker_t* worker)
{
    uint64_t now = now_ms();
    if(now == UINT64_MAX) {
        return -1;
    }

    for (size_t i = 0; i < worker->n_connections; ++i) {
        ls_connection_t* conn = worker->connections[i];
        if (conn->expire_at <= now) {
            conn->closed = 1;
        }
    }
    return 0;
}

/**
 * @brief Closes connections on a worker thread, by finalising the closing of all connections in this function less code is repeated and will make errors caused by closing connections incorrectly less likely
 * @param worker Pointer to the worker thread which connections will be expired on
 * @return On success 0, On failure -1
 */
static int ls_close_connections(ls_worker_t* worker)
{
    ls_log_cfg_t* log_cfg = worker->server->log_cfg;
    for (size_t i = 0; i < worker->n_connections; ++i) {
        ls_connection_t* conn = worker->connections[i];
        if(conn->closed) {
            /* Logs disconnect if the server is configured to do so */
            ls_log_disconnect(conn, log_cfg);
            epoll_ctl(worker->epfd, EPOLL_CTL_DEL, conn->fd, NULL);
            close(conn->fd);

            /* NOTICE: This is http specific right now but in the future if I add more protocols I will likely just make some protocol_ctx template with a memory pool for the context
             * and a void* to the specific protocol context struct as almost all protocols will need to dynamically allocate memory and if not its only one wasted pointer to the memory pool
             */

            /* Free all of the memory associated with the connection */
            ls_http_ctx_t* http_ctx = (ls_http_ctx_t*)conn->protocol_ctx;
            ls_free_pool(http_ctx->pool);
            ls_free_pool(conn->pool);
            /* Swap this now expired connection with the last valid connection */
            size_t last = worker->n_connections - 1;
            worker->connections[i] = worker->connections[last];
            worker->connections[i]->index = i;
            worker->connections[last] = conn;
            worker->n_connections--;
            /* Decrement the iterator or the element we just swapped to this position would be missed */
            --i;
        }
    }
    return 0;
}

/**
 * @brief runs the event loop for a worker thread
 * @param worker Pointer to the worker thread which the epoll loop is running on
 */
static void run_event_loop(ls_worker_t* worker) {
    /* Array to store events fetched by epoll_wait */
    struct epoll_event events[1024];
    ls_log_cfg_t* log_cfg = worker->server->log_cfg;

    while (1) {
        //printf("Open file descriptors: %d\n", count_open_fds());
        /* Compute maximum timeout to wait for events before moving on to expire connections */
        int timeout = compute_epoll_timeout(worker);
        if(timeout == -67) {
            const char* msg  = "WARNING: compute_epoll_timeout failed\n";
            ls_log_write(log_cfg, msg, strlen(msg), LS_LOG_ERROR);
        }

        /* Wait for epoll to return any events to be handled or until timeout is reached */
        int n = epoll_wait(worker->epfd, events, 1024, timeout);
        if (n == -1 && errno != EINTR) {
            const char* msg = "ERROR: epoll_wait failed and errno != EINTR, this SHOULD NOT happen\n";
            ls_log_write(log_cfg, msg, strlen(msg), LS_LOG_ERROR);
            return;
        }

        /* Handle every event fetched by epoll_wait */
        for (int i = 0; i < n; ++i) {
            ls_event_t* ev = (ls_event_t*)events[i].data.ptr;
            if (ev && ev->handler) {
                ev->handler(ev);
            }
            else {
                const char* msg = "ERROR: Event fetched from epoll was NULL or its handler was\n";
                ls_log_write(log_cfg, msg, strlen(msg), LS_LOG_ERROR);
            }
        }

        ls_expire_connections(worker);
        ls_close_connections(worker);
    }
}

/* I'm gonna call resource test.c even though it'll be function not source files but doens't matter*/
static int test(ls_http_request_t* req, ls_http_response_t* res, ls_mem_pool_t* pool)
{
    /* Let's try actually doing something now! */
    printf("IN test()\n");
    res->response = ls_palloc(pool, MAX_RESPONSE_SIZE);
    res->file_fd = -1; 
    res->status = 201;
    ls_write_status_line(res, 1, 1, 201, "Created");
    /* req_end points to end of headers so should be start of the body */
    u_char* req_end = req->request_end;
    int content_length = 0;

    ls_array_t* header_arr = req->headers;
    ls_header_t* curr = header_arr->head;
    size_t idx = 0;

    /* Iterate over all resources associated with a user supplied function and compare with the one in the request */
    while(idx < header_arr->n_elmnts)
    {
        curr = &((ls_header_t*)header_arr->head)[idx];
        if(curr->header_id == LS_HTTP_HDR_CONTENT_LENGTH) {
            int len = curr->value_end - curr->value_start;
            char* foo = ls_palloc(pool, len + 1); /* +1 for null terminator */
            snprintf(foo, len + 1, "%.*s\0", len, curr->value_start); 
            content_length = atoi(foo);
            printf("%s\n", foo);
            if(content_length == 0) {
                fprintf(stderr, "error converting content length to integer value\n");
                return -1;
            }
            break;
        }
        curr += sizeof(ls_header_t); 
        ++idx; 
    }
    
    char user_data[100];
    snprintf(user_data, 100, "%.*s\n", content_length, req_end);
    
    /* Hard code for testing - dont bother with checks for now */
    int fd = open("/home/reecet/Documents/cv-site/post_test.txt", O_APPEND | O_WRONLY);
    ssize_t n = write(fd, user_data, strlen(user_data));   
    
    printf("SHOULD HAVE WORKED\n");

    return 0; /* 0 to indicate success can use -1 to indicate fail */
}


int main()
{
    /* Increase file descriptor limit for this process */
    struct rlimit limits;

    getrlimit(RLIMIT_NOFILE, &limits);

    limits.rlim_cur = 16384;
    limits.rlim_max = 32768;

    if (setrlimit(RLIMIT_NOFILE, &limits) == -1) {
        perror("setrlimit");
        return -1;
    }

    struct sigaction sa = {0};
    sa.sa_handler = SIG_IGN;
    sigaction(SIGPIPE, &sa, NULL);

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
    /* LS_ENABLE_LOGGING | LS_LOG_INFO | LS_LOG_DEBUG | LS_LOG_ERROR | LS_LOG_WARNING*/
    if(ls_log_init(0, "www/testlog.log", server_context) == -1) {
        fprintf(stderr, "ERROR: Failed to initialise the logging module");
        return -1;
    }
    /* ================================================== FROM THIS POINT ON USE LOGGING TO RECORD INFORMATION E.G. ERRORS  ================================================== */
    /* Create a worker for the server */
    ls_mem_pool_t* worker_pool = ls_init_mem_pool(LS_DEFAULT_BLOCK_SIZE);
    if(worker_pool == NULL) {
        const char* msg = "ERROR: Failed to create memory pool for server worker\n";
        ls_log_write(server_context->log_cfg, msg, strlen(msg), LS_LOG_ERROR);
        return -1;
    }
    ls_worker_t* worker = ls_palloc(worker_pool, sizeof(ls_worker_t));
    if(worker == NULL) {
        const char* msg = "ERROR: Failed to allocate memory to create server worker\n";
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
        const char* msg = "ERROR: Failed to allocate memory to initialise connection pool for server worker\n";
        ls_log_write(server_context->log_cfg, msg, strlen(msg), LS_LOG_ERROR);
        return -1;
    }
    for(size_t i=0; i<worker->max_connections; ++i){
        worker->connections[i] = ls_palloc(worker_pool, sizeof(ls_connection_t));
        if(worker->connections[i] == NULL){
            const char* msg = "ERROR: Failed to allocate memory when creating a connection for the connection pool\n";
            ls_log_write(server_context->log_cfg, msg, strlen(msg), LS_LOG_ERROR);
            return -1;
        }
    }

    /* Create the array of listening sockets */
    server_context->lstning_sockets = ls_create_array(server_pool, sizeof(ls_lstning_sock_t), 1);
    if(server_context->lstning_sockets == NULL) {
        const char* msg = "ERROR: Failed to create array for server's listening sockets\n";
        ls_log_write(server_context->log_cfg, msg, strlen(msg), LS_LOG_ERROR);
        return -1;
    }

    /* Create the array of resource to function pointers */
    server_context->user_functions = ls_create_array(server_pool, sizeof(ls_resource_function_t), 1);
    if(server_context->user_functions == NULL) {
        const char* msg = "ERROR: Failed to create array for server's resource to user supplied function mapping\n";
        ls_log_write(server_context->log_cfg, msg, strlen(msg), LS_LOG_ERROR);
        return -1;
    }
    /* Push something to it to test it out */
    ls_resource_function_t* resource_function = (ls_resource_function_t*)ls_array_push(server_context->user_functions);
    *resource_function = (ls_resource_function_t){
        .resource = "/test.c",
        .user_function = test
    };
    
    


    /* Configure the socket to listen on the worker */
    ls_lstning_sock_t* sock = (ls_lstning_sock_t*)ls_array_push(server_context->lstning_sockets);
    if(sock == NULL) {
        const char* msg = "ERROR: Failed to push listening socket to the server's array\n";
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
        const char* msg = "ERROR: Failed to create listening socket to start server\n";
        ls_log_write(server_context->log_cfg, msg, strlen(msg), LS_LOG_ERROR);
        return -1;
    }

    /* Set epoll up for the worker */
    int epfd = epoll_create1(0);
    if (epfd == -1) {
        const char* msg = "ERROR: Failed to create epoll instance for server\n";
        ls_log_write(server_context->log_cfg, msg, strlen(msg), LS_LOG_ERROR);
        return -1;
    }

    /* Assign epfd to the worker */
    worker->epfd = epfd;

    /* Create event struct used for accepting connections on the listening socket */
    ls_event_t* ev = ls_palloc(worker_pool, sizeof(ls_event_t));
    if(ev == NULL) {
        const char* msg = "ERROR: Failed to allocate memory for listening socket accept handler\n";
        ls_log_write(server_context->log_cfg, msg, strlen(msg), LS_LOG_ERROR);
        return -1;
    }
    ev->data = sock;
    ev->handler = ls_accept_handler;

    /* Create the epoll_event and attach the event struct to it so we can call the handler and get the necessary data */
    struct epoll_event ee;
    ee.events = EPOLLIN;
    ee.data.ptr = ev;

    /* Monitor the epoll_event in the epoll loop */
    if (epoll_ctl(epfd, EPOLL_CTL_ADD, sock->fd, &ee) == -1) {
        const char* msg = "ERROR: Failed to add listening socket to be monitored by epoll instance\n";
        ls_log_write(server_context->log_cfg, msg, strlen(msg), LS_LOG_ERROR);
        return -1;
    }

    /* Set the root directory where web server's files will be accessed from */
    char abs_root[PATH_MAX];
    if(realpath("www/", abs_root) == NULL) {

        const char* msg = "ERROR: Failed to resolve the path for the servers root directory\n";
        ls_log_write(server_context->log_cfg, msg, strlen(msg), LS_LOG_ERROR);
        return -1;
    }

    ls_set_root_dir(server_context, abs_root);
    server_context->root_fd = open(abs_root, O_RDONLY | O_DIRECTORY);

    /* Run the event loop */
    run_event_loop(worker);

    /* Clean everything up */
    ls_log_close(server_context->log_cfg);
    ls_free_pool(server_pool);
    ls_free_pool(worker_pool);
    printf("CLOSING SERVER \n");
    return 0;
}

