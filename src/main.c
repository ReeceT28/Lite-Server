#include <netinet/in.h>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <unistd.h>
#include "ls_server.h"
#include "ls_http_parser.h"
#include "ls_mem_pool.h"
#include "ls_http_parser_test.h"
#include "ls_connection.h"

int main()
{
    ls_http_parser_init();
    ls_init_alloc();    

    // ls_lstning_sock_t sock;
    // if(ls_create_lstning_sock(&sock) == -1) {
    //     printf("Error creating listening socket \n");
    //     return -1;
    // }

    // ls_mem_pool_t* pool =  ls_init_mem_pool(LS_DEFAULT_BLOCK_SIZE);
    
    // ls_server_context_t* server_context = ls_palloc(pool, sizeof(ls_server_context_t));


    // ls_free_pool(pool);

    // parser_run_print_test();
    parser_run_benchmark(1000000);

    // int epfd = epoll_create1(0);
    return 0;
}

/*
 * NEXT STEPS:
 * Prevent Directory Traversal by using more secure functions like openat2(),
 * can take inspiration from
 * https://github.com/google/safeopen/blob/66b54d5181c6fee7ceb20782daca922b24f62819/safeopen_linux.go
 * Handle multiple clients, look into Having Open connections
 * Support more methods like POST
 * Non blocking accepting of connections
 * Making public
 * */
