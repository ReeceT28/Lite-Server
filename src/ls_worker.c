#include <string.h>
#include <sys/epoll.h>
#include "ls_connection.h"
#include "ls_server.h"
#include "ls_logging.h"
#include "ls_worker.h"

ls_worker_t* ls_create_worker(ls_server_context_t* server_context, size_t max_connections)
{
    ls_mem_pool_t* worker_pool = ls_init_mem_pool(LS_DEFAULT_BLOCK_SIZE);
    if(worker_pool == NULL) {
        const char* msg = "ERROR: Failed to create memory pool for server worker\n";
        ls_log_write(server_context->log_cfg, msg, strlen(msg), LS_LOG_ERROR);
        return NULL;
    }
    ls_worker_t* worker = ls_palloc(worker_pool, sizeof(ls_worker_t));
    if(worker == NULL) {
        const char* msg = "ERROR: Failed to allocate memory to create server worker\n";
        ls_log_write(server_context->log_cfg, msg, strlen(msg), LS_LOG_ERROR);
        return NULL;
    }
    worker->server = server_context;
    worker->pool = worker_pool;
    worker->max_connections = max_connections;
    worker->n_connections = 0;
    /* This approach might eat up a lot of memory but I think it should still be negligible for a modern system and I think is a good approach for prioritising fast responses */
    worker->connections = ls_palloc(worker_pool, sizeof(ls_connection_t*) * worker->max_connections);
    if(worker->connections == NULL) {
        const char* msg = "ERROR: Failed to allocate memory to initialise connection pool for server worker\n";
        ls_log_write(server_context->log_cfg, msg, strlen(msg), LS_LOG_ERROR);
        return NULL;
    }
    for(size_t i=0; i<worker->max_connections; ++i){
        worker->connections[i] = ls_palloc(worker_pool, sizeof(ls_connection_t));
        if(worker->connections[i] == NULL){
            const char* msg = "ERROR: Failed to allocate memory when creating a connection for the connection pool\n";
            ls_log_write(server_context->log_cfg, msg, strlen(msg), LS_LOG_ERROR);
            return NULL;
        }
    }

    /* Create the array of listening sockets */
    server_context->lstning_sockets = ls_create_array(server_context->pool, sizeof(ls_lstning_sock_t), 1);
    if(server_context->lstning_sockets == NULL) {
        const char* msg = "ERROR: Failed to create array for server's listening sockets\n";
        ls_log_write(server_context->log_cfg, msg, strlen(msg), LS_LOG_ERROR);
        return NULL;
    }

    /* Set epoll up for the worker */
    int epfd = epoll_create1(0);
    if (epfd == -1) {
        const char* msg = "ERROR: Failed to create epoll instance for server\n";
        ls_log_write(server_context->log_cfg, msg, strlen(msg), LS_LOG_ERROR);
        return NULL;
    }

    /* Assign epfd to the worker */
    worker->epfd = epfd;

    return worker;
}

ls_lstning_sock_t* ls_add_listener(ls_worker_t* worker, ls_socket_conf_t socket_conf)
{
    ls_server_context_t* server_context = worker->server;
    /* Configure the socket to listen on the worker */
    ls_lstning_sock_t* sock = (ls_lstning_sock_t*)ls_array_push(server_context->lstning_sockets);
    if(sock == NULL) {
        const char* msg = "ERROR: Failed to push listening socket to the server's array\n";
        ls_log_write(server_context->log_cfg, msg, strlen(msg), LS_LOG_ERROR);
        return NULL;
    }
    sock->worker = worker;
    sock->config = socket_conf;

    /* Actually creates the listening socket and stores fd under sock->fd */
    if(ls_create_lstning_sock(sock) == -1) {
        const char* msg = "ERROR: Failed to create listening socket to start server\n";
        ls_log_write(server_context->log_cfg, msg, strlen(msg), LS_LOG_ERROR);
        return NULL;
    }


    /* Create event struct used for accepting connections on the listening socket */
    ls_event_t* ev = ls_palloc(worker->pool, sizeof(ls_event_t));
    if(ev == NULL) {
        const char* msg = "ERROR: Failed to allocate memory for listening socket accept handler\n";
        ls_log_write(server_context->log_cfg, msg, strlen(msg), LS_LOG_ERROR);
        return NULL;
    }
    ev->data = sock;
    ev->handler = ls_accept_handler;

    /* Create the epoll_event and attach the event struct to it so we can call the handler and get the necessary data */
    struct epoll_event ee;
    ee.events = EPOLLIN;
    ee.data.ptr = ev;

    /* Monitor the epoll_event in the epoll loop */
    if (epoll_ctl(worker->epfd, EPOLL_CTL_ADD, sock->fd, &ee) == -1) {
        const char* msg = "ERROR: Failed to add listening socket to be monitored by epoll instance\n";
        ls_log_write(server_context->log_cfg, msg, strlen(msg), LS_LOG_ERROR);
        return NULL;
    }

    return sock;
}
