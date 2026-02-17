#include "http_response.h"
#include <stdio.h>
#include <stdlib.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
#include <string.h>
#include <time.h>
/*
 * NEXT STEPS:
 * Prevent Directory Traversal by using more secure functions like openat2(), can take inspiration from
 * https://github.com/google/safeopen/blob/66b54d5181c6fee7ceb20782daca922b24f62819/safeopen_linux.go
 * Handle multiple clients, look into Having Open connections
 * Support more methods like POST
 * Non blocking accepting of connections
 * Making public
 * */

void test_request_parse()
{
    http_request req;

    req.raw_request = "GET /cookies HTTP/1.1\r\n"
                      "Host: 127.0.0.1:8090\r\n"
                      "Connection: keep-alive\r\n"
                      "Cache-Control: max-age=0\r\n"
                      "Accept: text/html,application/xhtml+xml,application/xml;q=0.9,*/*;q=0.8\r\n"
                      "User-Agent: Mozilla/5.0 (Windows NT 6.1; WOW64) AppleWebKit/537.17 (KHTML, like Gecko) Chrome/24.0.1312.56 Safari/537.17\r\n"
                      "Accept-Encoding: gzip,deflate,sdch\r\n"
                      "Accept-Language: en-US,en;q=0.8\r\n"
                      "Accept-Charset: ISO-8859-1,utf-8;q=0.7,*;q=0.3\r\n"
                      "Cookie: name=wookie\r\n\r\n";

    req.request_len = strlen(req.raw_request);
    int err_code = 0;

    parse_request(&req, &err_code);

    // Uncomment if you want to print request info (slow!)
    // print_request_info(&req);
}

int send_all(int clientSocket, const char* buf, size_t buf_len)
{
    const char* ptr = buf;
    ssize_t remaining = buf_len;
    while(remaining > 0)
    {
        ssize_t sent = send(clientSocket, ptr, remaining, 0);
        if(sent<0)
        {
            return -1;
        }
        ptr += sent;
        remaining -= sent;
    }
    return 0;
}

int send_http_response(int client_fd, http_response* res)
{
    if(send_all(client_fd, res->response_head, res->response_head_len) < 0)
        return -1;

    if(res->body_file)
    {
        char buf[SEND_BUF_SIZE];
        size_t bytes_read;
        while((bytes_read = fread(buf, 1, sizeof buf, res->body_file)) > 0)
        {
            if(send_all(client_fd, buf, bytes_read) < 0)
                return -1;

        }
        fclose(res->body_file);
    }
    return 0;
}

int main()
{
    const int num_tests = 10;
    const int num_iterations = 10000000;

    double total_elapsed = 0.0;
    double min_elapsed = 1e30;
    double max_elapsed = 0.0;

    printf("\n=========== Performance Tests ===========\n\n");

    for (int i = 0; i < num_tests; ++i)
    {
        clock_t start_time = clock();

        for (int j = 0; j < num_iterations; ++j)
        {
            test_request_parse();
        }

        clock_t end_time = clock();

        double elapsed_secs =
            (double)(end_time - start_time) / CLOCKS_PER_SEC;

        double ns_per_request =
            (elapsed_secs * 1e9) / num_iterations;

        double req_per_sec =
            num_iterations / elapsed_secs;

        printf("Test %2d: %.3f sec | %.2f ns/request | %.0f req/sec\n",
               i + 1,
               elapsed_secs,
               ns_per_request,
               req_per_sec);

        total_elapsed += elapsed_secs;

        if (elapsed_secs < min_elapsed)
            min_elapsed = elapsed_secs;

        if (elapsed_secs > max_elapsed)
            max_elapsed = elapsed_secs;
    }

    double avg_elapsed = total_elapsed / num_tests;
    double avg_ns = (avg_elapsed * 1e9) / num_iterations;
    double avg_rps = num_iterations / avg_elapsed;

    printf("\n=========== Summary ===========\n\n");
    printf("Tests run:        %d\n", num_tests);
    printf("Iterations/test:  %d\n\n", num_iterations);

    printf("Average time:     %.3f sec\n", avg_elapsed);
    printf("Average latency:  %.2f ns/request\n", avg_ns);
    printf("Average throughput: %.0f req/sec\n\n", avg_rps);

    printf("Best test:        %.3f sec\n", min_elapsed);
    printf("Worst test:       %.3f sec\n\n", max_elapsed);

    return 0;
    

    // Useful reference for these functions: https://pubs.opengroup.org/onlinepubs/9799919799/
    // AF_INET for IPv4, SOCK_STREAM for socket type (used for TCP), 0for default protocol of for the requested socket type, in this case TCP
    int serverSocket = socket(AF_INET, SOCK_STREAM, 0);
    if (serverSocket == -1)
    {
        // Can use errno to get more specific errors
        return 1;
    }

    struct sockaddr_in serverAddr = {0};
    serverAddr.sin_family = AF_INET;
    // htons converts from little-endian (how computers store multi-byte values) to big-endian (how network protocols store multi-byte values)
    serverAddr.sin_port = htons(8080);
    // INADDR_ANY means that we accept connection on all local network interfaces
    serverAddr.sin_addr.s_addr = INADDR_ANY;


    int opt = 1;
    setsockopt(serverSocket, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    // Associates a socket with an IP address and port number
    if (bind(serverSocket, (struct sockaddr *)&serverAddr, sizeof(serverAddr)) == -1)
    {
        // Can use errno to get more specific errors
        return 1;
    }

    // listen() tells OS to start listening for connection request on the given socket, second parameter is maximum number of pending connections the queue can hold
    if (listen(serverSocket, 5) == -1)
    {
        // Can use errno to get last error which correspond to specific errors
        return 1;
    }

    // accept() takes the connection request from the front of the queue generated by listen().
    // The OS creates another socket with the same socket type(SOCK_STREAM), protocol(TCP) and address family(IPv4). Returns the file descriptor.
    int clientSocket = accept(serverSocket, NULL, NULL);
    if (clientSocket == -1)
    {
        // Can use errno to get more specific errors
        return 1;
    }

    printf("Received Connection starting communication now \n");
    char buffer[1024];
    while(1)
    {
        ssize_t byteSize = recv(clientSocket, buffer, sizeof(buffer) - 1, 0);
        if(byteSize > 0)
        {
            buffer[byteSize] = '\0';
            http_request req;
            req.raw_request = buffer;
            req.request_len = strlen(buffer);
            int err_code = 0;
            parse_request(&req, &err_code);
            print_request_info(&req);
            http_response res;
            build_http_response(&res,&req,&err_code);
            print_response_info(&res);
            send_http_response(clientSocket, &res);
        }
        else if(byteSize == 0)
        {
            printf("Client Disconnected \n");
            break;
        }
        else
        {
            printf("Error in receiving transmission from client \n");
            break;
        }
    }

    return 0;
}
