#include <netinet/in.h>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <unistd.h>
#include "ls_http_parser.h"
#include "ls_mem_pool.h"

int main()
{
    ls_http_parser_init();
    ls_init_alloc();
    ls_mem_pool_t* pool =  ls_init_mem_pool(LS_DEFAULT_BLOCK_SIZE);
    for(size_t i = 0; i < 1000000; ++i) {
        size_t* i_ptr = ls_palloc(pool, sizeof(size_t));
        *i_ptr = i;
    }
    ls_free_pool(pool);
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
