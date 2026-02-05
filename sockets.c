#include <stdio.h>
#include <stdlib.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
#include <string.h>

typedef struct
{
    const char* name;
    size_t name_len;
    const char* value;
    size_t value_len;
} HttpHeader;

typedef struct
{
    // Request line
    const char* method;
    size_t method_len;
    const char* path;
    size_t path_len;
    const char* version;
    size_t version_len;

    HttpHeader headers[32];
    size_t header_count;

    const char* buf;
    size_t buf_len;
} HttpRequest;

typedef struct
{
    const char* version;
    size_t version_len;
    const char* reason;
    size_t reason_length;
    int status_code;

    HttpHeader headers[32];
    size_t header_count;

    char response[1024];
    size_t response_size;

} HttpResponse;


// Purpose: Parse a token
// Special Parameters: token_start - pointer to pointer of const char which points to start of token, token_len - length of token, delimiter
// Return Values: If success - buf. If fails - nullptr. Also modifies token_start and token_len so is effectively "returning" these
const char* parseToken(const char* buf, const char* buf_end, const char** token_start, size_t* token_len, char delimiter, int* err_code)
{
    if(buf == buf_end){ *err_code = -2; return NULL;}
    *token_start = buf;
    while(buf < buf_end)
    {
        if(*buf == delimiter) break;
        buf++;
    }
    *token_len = buf - *token_start;
    // Currently buf is at the last character of the token (the delimiter) but buf should point to next charater/ byte to be read
    if(buf < buf_end) buf++;

    return buf;
}

const char* skipOWS(const char* buf, const char* buf_end)
{
    while(buf < buf_end && (*buf == ' ' || *buf == '\t')) buf++;
    return buf;
}

// Purpose: Parse what is remaining of a line
const char* parseLine(const char* buf, const char* buf_end, const char** line_start, size_t* line_len,  int* err_code)
{
    if(buf == buf_end){ *err_code = -2; return NULL;}
    *line_start = buf;
    while(buf < buf_end)
    {
        if(*buf == '\r')
        {
            buf++;
            if(buf >= buf_end || *buf != '\n')
            {
                *err_code = -1;
                return NULL;
            }
            *line_len = buf - 1 - *line_start;
            return ++buf;
        }
        buf++;
    }
    *err_code = -2;
    return NULL;
}
// Purpose: Parse the request line
const char* parseReqLine(const char* buf, const char* buf_end, int* err_code, HttpRequest* req)
{
    buf = parseToken(buf, buf_end, &req->method, &req->method_len, ' ', err_code);
    buf = parseToken(buf, buf_end, &req->path, &req->path_len, ' ', err_code);
    buf = parseLine(buf, buf_end, &req->version, &req->version_len, err_code);
    if(*err_code != 0)
    {
        printf("There was an error during parsing the request line\n");
    }
    return buf;
}
const char* parseHeaders(const char* buf, const char* buf_end, int* err_code, HttpRequest* req)
{
    while(buf < buf_end)
    {
        // Check for blank line implying end of headers
        if(buf + 1 < buf_end && *buf == '\r' && *(buf+1) == '\n') return buf + 2;

        // Check for too many headers
        if(req->header_count >= sizeof(req->headers) / sizeof(req->headers[0]))
        {
            printf("Error: too many headers");
            *err_code = -3;
            return NULL;
        }

        HttpHeader* header = &req->headers[req->header_count];
        buf = parseToken(buf, buf_end, &header->name, &header->name_len, ':', err_code);
        // Iterate buff to skip the :
        buf++;
        // Deal with OWS
        buf = skipOWS(buf, buf_end);
        // Get the value
        header->value = buf;
        while(buf < buf_end && (*buf!= ' ' && *buf!= '\t' && *buf != '\r'))
            buf++;
        header->value_len = buf - header->value;
        // Deal with OWS if present
        buf = skipOWS(buf, buf_end);
        if(buf + 1 >= buf_end || !(*buf == '\r' && *(buf + 1) == '\n'))
        {
            // No CRLF for header then return error
            printf("Error: No CRLF at end of header");
            *err_code = -1;
            return NULL;
        }
        buf = buf + 2;
        req->header_count++;
    }
    printf("Finished parsing headers");
    return buf;
}

void parseRequest(HttpRequest* req, int* err_code)
{
    req->header_count = 0;
    const char* buf = req->buf;
    const char* buf_end = req->buf + req->buf_len;
    buf = parseReqLine(buf, buf_end, err_code, req);
    buf = parseHeaders(buf, buf_end, err_code, req);
}

void buildStatusLine(HttpResponse* res, HttpRequest* req, int* err_code)
{
    if(memcmp(req->version,"HTTP/1.1",req->version_len) != 0)
    {
        res->status_code = 505;
        res->reason = "HTTP Version Not Supported";
        res->reason_length = 26;
        return;
    }
}

void writeStatusLine(HttpResponse* res, HttpRequest* req, int* err_code)
{
    res->response_size += snprintf(
        res->response + res->response_size,
        sizeof(res->response) - res->response_size,
        "%.*s %d %.*s\r\n",
        (int)res->version_len,
        res->version,
        res->status_code,
        (int)res->reason_length,
        res->reason
    );
}

void addResource(const char* filePath, HttpResponse* res)
{
    FILE* fptr;
    fptr = fopen(filePath, "r");
    if (fptr == NULL)
    {
        res->status_code = 404;
        res->reason = "Not Found";
        res->reason_length = 9;
        return;
    }
    fseek(fptr, 0L, SEEK_END);
    size_t size = ftell(fptr);
    char str[256];
    snprintf(str, sizeof str, "%zu", size);
    HttpHeader contentLength = {.name = "Content-Length", .name_len = 14, .value = str, .value_len = strlen(str)};
    res->headers[res->header_count] = contentLength;
    res->header_count += 1;
}

void handleGet(HttpResponse* res, HttpRequest* req, int* err_code)
{
    char filePath[1024];
    if(memcmp(req->path,"/",1) == 0)
    {
        strcpy(filePath, "./www/index.html");
    }
    else
    {
        snprintf(filePath, sizeof(filePath), "./www%.s", req->path);
    }

    addResource(filePath, res);
}

void buildHttpResponse(HttpResponse* res, HttpRequest* req, int* err_code)
{
    // Assume status code of 200 so a valid request first
    res->status_code = 200;
    res->reason = "OK";
    res->reason_length = 2;
    res->response_size = 0;
    res->version = req->version;
    res->version_len = req->version_len;
    res->header_count = 0;

    if(memcmp(req->method,"GET",req->method_len) == 0)
    {
        handleGet(res, req, err_code);
    }
    else
    {
        res->status_code = 405;
        res->reason = "Method Not Allowed";
        res->reason_length = 18;
    }



    buildStatusLine(res, req, err_code);
    writeStatusLine(res, req, err_code);
    // NEED TO IMPLEMENT BUILDING HEADERS AND ADDING BODY
}

void printRequestInfo(HttpRequest* req)
{
    size_t i;

    printf("=== HTTP Request ===\n");

    /* Request line */
    printf("Method : %.*s\n",
           (int)req->method_len, req->method);

    printf("Path   : %.*s\n",
           (int)req->path_len, req->path);

    printf("Version: %.*s\n",
           (int)req->version_len, req->version);

    /* Headers */
    printf("\nHeaders (%zu):\n", req->header_count);

    for (i = 0; i < req->header_count; i++) {
        printf("  %.*s: %.*s\n",
               (int)req->headers[i].name_len,
               req->headers[i].name,
               (int)req->headers[i].value_len,
               req->headers[i].value);
    }

    /* Raw buffer info */
    printf("\nRaw buffer length: %zu bytes\n", req->buf_len);
}

void printResponseInfo(HttpResponse* res)
{
    printf("=== HTTP Response ===\n");
    printf("%.*s", (int)res->response_size, res->response);
}

void test()
{
    HttpRequest req;
    req.buf = "GET /example/page.html HTTP/1.1\r\nHost: www.example.com\r\nUser-Agent: MyCustomClient/1.0\r\nAccept: text/html\r\n\r\n\0";
    req.buf_len = strlen(req.buf);
    int err_code = 0;
    parseRequest(&req, &err_code);
    printRequestInfo(&req);
    HttpResponse res;
    buildHttpResponse(&res,&req,&err_code);
    printResponseInfo(&res);
}


int main()
{

    test();
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




    return 0;
}

