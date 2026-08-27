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
#include <pthread.h>
#include <unistd.h>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <unistd.h>
#include <signal.h>
#include "ls_server_loop.h"
#include "ls_worker.h"
#include "ls_http_parser.h"
#include "ls_mem_pool.h"
#include "ls_connection.h"
#include "ls_server.h"
#include "ls_logging.h"

static int test(ls_http_request_t* req, ls_http_response_t* res, ls_mem_pool_t* pool)
{
    (void)req; (void)res; (void)pool;
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

    ls_server_context_t* server_context = ls_create_server(0, "www/testlog.log", "../cv-site");
    ls_mem_pool_t* server_pool = server_context->pool;

    /* Push something to it to test it out */
    ls_resource_function_t* resource_function = (ls_resource_function_t*)ls_array_push(server_context->user_functions);
    *resource_function = (ls_resource_function_t){
        .resource = "/test.c",
        .user_function = test
    };

    /* On testing I figured out this will count number of hyperthreads so not necessarily number of cores but I think this should still be improve performance */
    long n_cores = sysconf(_SC_NPROCESSORS_ONLN);
    pthread_t threads[n_cores];
    ls_worker_t* workers[n_cores];

    for(long i = 0; i < n_cores; ++i) {
        /* Create a worker for the server */
        ls_worker_t* worker = ls_create_worker(server_context, 10000);
        workers[i] = worker;

        ls_lstning_sock_t* lstning_sock = ls_add_listener(
            worker,
            (ls_socket_conf_t){
                .family = AF_INET,
                .socktype = SOCK_STREAM,
                .host = NULL,
                .port = 8080,
                .backlog = SOMAXCONN,
                .type = LS_SOCK_HTTP
            }
        );
        (void)lstning_sock;

        pthread_create(&threads[i], NULL, run_server_loop_thread, worker);
        printf("Created thread: %ld\n", i + 1L);
    }

    /* Clean everything up */
    for(long i = 0; i < n_cores; ++i) {
        pthread_join(threads[i], NULL);
        free(workers[i]->pool);
    }

    ls_log_close(server_context->log_cfg);
    ls_free_pool(server_pool);
    return 0;
}
