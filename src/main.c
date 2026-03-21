#include <netinet/in.h>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <unistd.h>
#include "ls_http_parser.h"
#include "ls_http_parser_test.h"
#include "ls_mem_pool.h"
#include "ls_connection.h"

void run_event_loop(ls_server_context_t *server) {
    struct epoll_event events[1024];

    while (1) {
        int n = epoll_wait(server->epfd, events, 1024, -1);
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
    }
}

int main()
{
    ls_http_parser_init();
    ls_init_alloc();    

    /* Create memory pool and initialise server context */
    ls_mem_pool_t* pool =  ls_init_mem_pool(LS_DEFAULT_BLOCK_SIZE);
    ls_server_context_t* server_context = ls_palloc(pool, sizeof(ls_server_context_t));
    /* Add listening socket*/
    server_context->lstning_sockets = ls_create_array(pool, sizeof(ls_lstning_sock_t), 1);
    ls_lstning_sock_t* sock = (ls_lstning_sock_t*)ls_array_push(server_context->lstning_sockets);
    if(sock == NULL) {
        printf("Error pushing onto array for listening sockets \n");
    }
    sock->server = server_context;
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
    server_context->epfd = epfd;
    /* Create accept handler for */
    ls_event_t* ev = ls_palloc(pool, sizeof(ls_event_t));
    ev->data = sock;
    ev->handler = ls_accept_handler;

    struct epoll_event ee;
    ee.events = EPOLLIN;
    ee.data.ptr = ev;

    if (epoll_ctl(epfd, EPOLL_CTL_ADD, sock->fd, &ee) == -1) {
      perror("epoll_ctl");
      return -1;
    }

    run_event_loop(server_context);

    ls_free_pool(pool);
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
