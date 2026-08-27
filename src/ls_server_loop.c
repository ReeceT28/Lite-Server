#include <errno.h>
#include <string.h>
#include <sys/epoll.h>
#include <unistd.h>
#include "ls_logging.h"
#include "ls_connection.h"
#include "ls_utils.h"
#include "ls_server_loop.h"                     

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
void run_server_loop(ls_worker_t* worker) {
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

void* run_server_loop_thread(void* worker)
{
    ls_worker_t* w = (ls_worker_t*)worker;
    run_server_loop(w);
    return NULL;
}