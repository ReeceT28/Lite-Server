#include <cstring>
#include <iostream>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

int main() {
    // AF_INET for IPv4, SOCK_STREAM for TCP, 0  for default protocol of for the
    // requested socket type https://www.linuxhowtos.org/manpages/2/socket.htm
    // https://pubs.opengroup.org/onlinepubs/009696599/functions/socket.html
    int serverSocket = socket(AF_INET, SOCK_STREAM, 0);
    if (serverSocket == -1)
    {
        // Can use errno() to get last error which correspond to specific errors
        std::cerr << "socket() has returned an error\n";
        return 1;
    }

    sockaddr_in serverAddr;
    serverAddr.sin_family = AF_INET;
    // htons converts from little-endian (how computers store multi-byte values)
    // to big-endian (how network protocols store multi-byte values)
    serverAddr.sin_port = htons(8080);
    // INADDR_ANY means that we accept connection on all local network interfaces
    serverAddr.sin_addr.s_addr = INADDR_ANY;

    // https://pubs.opengroup.org/onlinepubs/009696599/functions/bind.html
    if (bind(serverSocket, (sockaddr *)&serverAddr, sizeof(serverAddr)) == -1)
    {
        // Can use errno() to get last error which correspond to specific errors
        std::cerr << "bind() has returned an error\n";
        return 1;
    }

    // https://pubs.opengroup.org/onlinepubs/009696599/functions/listen.html
    // listen() listens for socket connections
    if (listen(serverSocket, 5) == -1)
    {
        // Can use errno() to get last error which correspond to specific errors
        std::cerr << "listen() has returned an error\n";
        return 1;
    }

    std::cout << "The server socket is succesfully bound to all local IPv4 "
        "interfaces and is listening for TCP connections ...\n";

    int clientSocket = accept(serverSocket, nullptr, nullptr);
    if (clientSocket == -1)
    {
        std::cerr << "Accept failed \n";
        return 1;
    }

    const char *message = "Hello World!";
    send(clientSocket, message, std::strlen(message), 0);
    close(clientSocket);
    close(serverSocket);

    return 0;
}
