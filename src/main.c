#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
#include "ls_http_parser.h"

int main()
{
    ls_http_parser_init();
    return 0;
}

/*
 * NEXT STEPS:
 * Prevent Directory Traversal by using more secure functions like openat2(), can take inspiration from
 * https://github.com/google/safeopen/blob/66b54d5181c6fee7ceb20782daca922b24f62819/safeopen_linux.go
 * Handle multiple clients, look into Having Open connections
 * Support more methods like POST
 * Non blocking accepting of connections
 * Making public
 * */