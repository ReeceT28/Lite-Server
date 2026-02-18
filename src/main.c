#include <stdio.h>
#include <stdlib.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
#include <string.h>
#include <time.h>
#include "http_parser.h"

const char *http_requests[100] = {
    "GET /home HTTP/1.1",
    "GET /about HTTP/1.1",
    "GET /products HTTP/1.1",
    "GET /contact HTTP/1.1",
    "GET /blog HTTP/1.1",
    "GET /blog/123 HTTP/1.1",
    "GET /blog/124 HTTP/1.1",
    "GET /api/v1/items HTTP/1.1",
    "GET /api/v1/items/1 HTTP/1.1",
    "GET /api/v1/items/2 HTTP/1.1",
    "GET /images/logo.png HTTP/1.1",
    "GET /images/banner.jpg HTTP/1.1",
    "GET /scripts/main.js HTTP/1.1",
    "GET /styles/main.css HTTP/1.1",
    "GET /login HTTP/1.1",
    "GET /logout HTTP/1.1",
    "GET /dashboard HTTP/1.1",
    "GET /dashboard/stats HTTP/1.1",
    "GET /search?q=phone HTTP/1.1",
    "GET /search?q=laptop HTTP/1.1",
    "GET /faq HTTP/1.1",
    "GET /terms HTTP/1.1",
    "GET /privacy HTTP/1.1",
    "GET /api/v1/users HTTP/1.1",
    "GET /api/v1/users/1 HTTP/1.1",
    "GET /api/v1/users/2 HTTP/1.1",
    "GET /api/v1/orders HTTP/1.1",
    "GET /api/v1/orders/1 HTTP/1.1",
    "GET /api/v1/orders/2 HTTP/1.1",
    "GET /shop HTTP/1.1",
    "GET /shop/category/electronics HTTP/1.1",
    "GET /shop/category/clothing HTTP/1.1",
    "GET /shop/product/1 HTTP/1.1",
    "GET /shop/product/2 HTTP/1.1",
    "GET /shop/product/3 HTTP/1.1",
    "GET /cart HTTP/1.1",
    "GET /checkout HTTP/1.1",
    "GET /notifications HTTP/1.1",
    "GET /messages HTTP/1.1",
    "GET /messages/1 HTTP/1.1",
    "GET /messages/2 HTTP/1.1",
    "GET /api/v1/reports HTTP/1.1",
    "GET /api/v1/reports/2026 HTTP/1.1",
    "GET /downloads/manual.pdf HTTP/1.1",
    "GET /downloads/app.exe HTTP/1.1",
    "GET /status HTTP/1.1",
    "GET /health HTTP/1.1",
    "GET /sitemap.xml HTTP/1.1",
    "GET /robots.txt HTTP/1.1",
    "GET /news HTTP/1.1",
    "GET /news/1 HTTP/1.1",
    "GET /news/2 HTTP/1.1",
    "GET /news/3 HTTP/1.1",
    "GET /events HTTP/1.1",
    "GET /events/1 HTTP/1.1",
    "GET /events/2 HTTP/1.1",
    "GET /gallery HTTP/1.1",
    "GET /gallery/photo1.jpg HTTP/1.1",
    "GET /gallery/photo2.jpg HTTP/1.1",
    "GET /gallery/photo3.jpg HTTP/1.1",
    "GET /support HTTP/1.1",
    "GET /support/ticket/1 HTTP/1.1",
    "GET /support/ticket/2 HTTP/1.1",
    "GET /support/ticket/3 HTTP/1.1",
    "GET /forum HTTP/1.1",
    "GET /forum/thread/1 HTTP/1.1",
    "GET /forum/thread/2 HTTP/1.1",
    "GET /forum/thread/3 HTTP/1.1",
    "GET /forum/thread/4 HTTP/1.1",
    "GET /forum/thread/5 HTTP/1.1",
    "GET /forum/thread/6 HTTP/1.1",
    "GET /forum/thread/7 HTTP/1.1",
    "GET /forum/thread/8 HTTP/1.1",
    "GET /forum/thread/9 HTTP/1.1",
    "GET /forum/thread/10 HTTP/1.1",
    "GET /blog/tag/tech HTTP/1.1",
    "GET /blog/tag/science HTTP/1.1",
    "GET /blog/tag/lifestyle HTTP/1.1",
    "POST /login HTTP/1.1",
    "POST /logout HTTP/1.1",
    "POST /api/v1/items HTTP/1.1",
    "POST /api/v1/users HTTP/1.1",
    "POST /api/v1/orders HTTP/1.1",
    "POST /contact HTTP/1.1",
    "POST /newsletter/subscribe HTTP/1.1",
    "POST /shop/cart/add HTTP/1.1",
    "POST /shop/cart/remove HTTP/1.1",
    "POST /support/ticket HTTP/1.1",
    "POST /api/v1/feedback HTTP/1.1",
    "POST /api/v1/comments HTTP/1.1",
    "POST /search HTTP/1.1",
    "POST /register HTTP/1.1",
    "POST /checkout HTTP/1.1",
    "POST /api/v1/login HTTP/1.1",
    "PUT /api/v1/items/1 HTTP/1.1",
    "PUT /api/v1/items/2 HTTP/1.1",
    "DELETE /api/v1/items/1 HTTP/1.1",
    "PATCH /api/v1/items/2 HTTP/1.1",
    "OPTIONS /api/v1/items HTTP/1.1",
    "OPTIONS /api/v1/items HTTP/1.1"
};

size_t count = 0;
size_t option_count = 0;

void test_request_parse()
{
    http_request req;
    lite_server_config *conf;
    int err_code;

    req.raw_request = (const u_char*)http_requests[count % 100];
    req.request_len = strlen((const char*)req.raw_request);
    req.header_count = 0;
    const u_char* cursor = req.raw_request;
    const u_char* end = cursor + req.request_len;
    cursor = parse_request_line_op1(cursor, end, &req, conf, &err_code);
    if(req.method == LS_HTTP_OPTIONS)
    {
        option_count++;
    }
}


int main()
{
    const int num_tests = 1;
    const int num_iterations = 5000000;

    for (int i = 0; i < num_tests; ++i)
    {
        count = 0;
        for (int j = 0; j < num_iterations; ++j)
        {
            test_request_parse();
            count++;
        }
    }

    // printf("Option count: %zu", option_count);
    return 0;
}




// 
// int send_all(int clientSocket, const char* buf, size_t buf_len)
// {
//     const char* ptr = buf;
//     ssize_t remaining = buf_len;
//     while(remaining > 0)
//     {
//         ssize_t sent = send(clientSocket, ptr, remaining, 0);
//         if(sent<0)
//         {
//             return -1;
//         }
//         ptr += sent;
//         remaining -= sent;
//     }
//     return 0;
// }
// 
// int send_http_response(int client_fd, http_response* res)
// {
//     if(send_all(client_fd, res->response_head, res->response_head_len) < 0)
//         return -1;
// 
//     if(res->body_file)
//     {
//         char buf[SEND_BUF_SIZE];
//         size_t bytes_read;
//         while((bytes_read = fread(buf, 1, sizeof buf, res->body_file)) > 0)
//         {
//             if(send_all(client_fd, buf, bytes_read) < 0)
//                 return -1;
// 
//         }
//         fclose(res->body_file);
//     }
//     return 0;
// }
// 
//     // Useful reference for these functions: https://pubs.opengroup.org/onlinepubs/9799919799/
//     // AF_INET for IPv4, SOCK_STREAM for socket type (used for TCP), 0for default protocol of for the requested socket type, in this case TCP
//     int serverSocket = socket(AF_INET, SOCK_STREAM, 0);
//     if (serverSocket == -1)
//     {
//         // Can use errno to get more specific errors
//         return 1;
//     }
// 
//     struct sockaddr_in serverAddr = {0};
//     serverAddr.sin_family = AF_INET;
//     // htons converts from little-endian (how computers store multi-byte values) to big-endian (how network protocols store multi-byte values)
//     serverAddr.sin_port = htons(8080);
//     // INADDR_ANY means that we accept connection on all local network interfaces
//     serverAddr.sin_addr.s_addr = INADDR_ANY;
// 
// 
//     int opt = 1;
//     setsockopt(serverSocket, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
// 
//     // Associates a socket with an IP address and port number
//     if (bind(serverSocket, (struct sockaddr *)&serverAddr, sizeof(serverAddr)) == -1)
//     {
//         // Can use errno to get more specific errors
//         return 1;
//     }
// 
//     // listen() tells OS to start listening for connection request on the given socket, second parameter is maximum number of pending connections the queue can hold
//     if (listen(serverSocket, 5) == -1)
//     {
//         // Can use errno to get last error which correspond to specific errors
//         return 1;
//     }
// 
//     // accept() takes the connection request from the front of the queue generated by listen().
//     // The OS creates another socket with the same socket type(SOCK_STREAM), protocol(TCP) and address family(IPv4). Returns the file descriptor.
//     int clientSocket = accept(serverSocket, NULL, NULL);
//     if (clientSocket == -1)
//     {
//         // Can use errno to get more specific errors
//         return 1;
//     }
// 
//     printf("Received Connection starting communication now \n");
//     char buffer[1024];
//     while(1)
//     {
//         ssize_t byteSize = recv(clientSocket, buffer, sizeof(buffer) - 1, 0);
//         if(byteSize > 0)
//         {
//             buffer[byteSize] = '\0';
//             http_request req;
//             req.raw_request = buffer;
//             req.request_len = strlen(buffer);
//             int err_code = 0;
//             parse_request(&req, &err_code);
//             print_request_info(&req);
//             http_response res;
//             build_http_response(&res,&req,&err_code);
//             print_response_info(&res);
//             send_http_response(clientSocket, &res);
//         }
//         else if(byteSize == 0)
//         {
//             printf("Client Disconnected \n");
//             break;
//         }
//         else
//         {
//             printf("Error in receiving transmission from client \n");
//             break;
//         }
//     }
// 
//     return 0;

/*
 * NEXT STEPS:
 * Prevent Directory Traversal by using more secure functions like openat2(), can take inspiration from
 * https://github.com/google/safeopen/blob/66b54d5181c6fee7ceb20782daca922b24f62819/safeopen_linux.go
 * Handle multiple clients, look into Having Open connections
 * Support more methods like POST
 * Non blocking accepting of connections
 * Making public
 * */