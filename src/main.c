#include <stdio.h>
#include <stdlib.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
#include <string.h>
#include <time.h>
#include "ls_http_request.h"
#include "http_parser.h"
#include "ls_hash.h"

#define BIG_REQ                                                                                                                        \
    "GET /wp-content/uploads/2010/03/hello-kitty-darth-vader-pink.jpg HTTP/1.1\r\n"                                                \
    "Host: www.kittyhell.com\r\n"                                                                                                  \
    "User-Agent: Mozilla/5.0 (Macintosh; U; Intel Mac OS X 10.6; ja-JP-mac; rv:1.9.2.3) Gecko/20100401 Firefox/3.6.3 "             \
    "Pathtraq/0.9\r\n"                                                                                                             \
    "Accept: text/html,application/xhtml+xml,application/xml;q=0.9,*/*;q=0.8\r\n"                                                  \
    "Accept-Language: ja,en-us;q=0.7,en;q=0.3\r\n"                                                                                 \
    "Accept-Encoding: gzip,deflate\r\n"                                                                                            \
    "Accept-Charset: Shift_JIS,utf-8;q=0.7,*;q=0.7\r\n"                                                                            \
    "Keep-Alive: 115\r\n"                                                                                                          \
    "Connection: keep-alive\r\n"                                                                                                   \
    "Cookie: wp_ozh_wsa_visits=2; wp_ozh_wsa_visit_lasttime=xxxxxxxxxx; "                                                          \
    "__utma=xxxxxxxxx.xxxxxxxxxx.xxxxxxxxxx.xxxxxxxxxx.xxxxxxxxxx.x; "                                                             \
    "__utmz=xxxxxxxxx.xxxxxxxxxx.x.x.utmccn=(referral)|utmcsr=reader.livedoor.com|utmcct=/reader/|utmcmd=referral\r\n"             \
    "\r\n"

const char *ls_http_request_complete[100] = {
    "GET /home HTTP/1.1\r\nHost: example.com\r\nConnection: keep-alive\r\n\r\n",
    "GET /about HTTP/1.1\r\nHost: example.com\r\nAccept: text/html\r\n\r\n",
    "GET /products HTTP/1.1\r\nHost: example.com\r\nConnection: close\r\n\r\n",
    "GET /contact HTTP/1.1\r\nHost: example.com\r\nReferer: http://example.com\r\n\r\n",
    "GET /blog HTTP/1.1\r\nHost: example.com\r\nCookie: sessionid=abc123\r\n\r\n",
    "GET /blog/123 HTTP/1.1\r\nHost: example.com\r\nConnection: keep-alive\r\n\r\n",
    "GET /blog/124 HTTP/1.1\r\nHost: example.com\r\nAccept: text/html\r\n\r\n",
    "GET /api/v1/items HTTP/1.1\r\nHost: api.example.com\r\nOrigin: http://example.com\r\n\r\n",
    "GET /api/v1/items/1 HTTP/1.1\r\nHost: api.example.com\r\nConnection: close\r\n\r\n",
    "GET /api/v1/items/2 HTTP/1.1\r\nHost: api.example.com\r\nCookie: token=xyz\r\n\r\n",
    "GET /images/logo.png HTTP/1.1\r\nHost: static.example.com\r\n\r\n",
    "GET /images/banner.jpg HTTP/1.1\r\nHost: static.example.com\r\nConnection: keep-alive\r\n\r\n",
    "GET /scripts/main.js HTTP/1.1\r\nHost: static.example.com\r\n\r\n",
    "GET /styles/main.css HTTP/1.1\r\nHost: static.example.com\r\n\r\n",
    "GET /login HTTP/1.1\r\nHost: example.com\r\nAccept: text/html\r\n\r\n",
    "GET /logout HTTP/1.1\r\nHost: example.com\r\nConnection: close\r\n\r\n",
    "GET /dashboard HTTP/1.1\r\nHost: example.com\r\nCookie: sessionid=xyz789\r\n\r\n",
    "GET /dashboard/stats HTTP/1.1\r\nHost: example.com\r\nAccept: application/json\r\n\r\n",
    "GET /search?q=phone HTTP/1.1\r\nHost: example.com\r\nConnection: keep-alive\r\n\r\n",
    "GET /search?q=laptop HTTP/1.1\r\nHost: example.com\r\nConnection: keep-alive\r\n\r\n",
    "GET /faq HTTP/1.1\r\nHost: example.com\r\n\r\n",
    "GET /terms HTTP/1.1\r\nHost: example.com\r\n\r\n",
    "GET /privacy HTTP/1.1\r\nHost: example.com\r\n\r\n",
    "GET /api/v1/users HTTP/1.1\r\nHost: api.example.com\r\n\r\n",
    "GET /api/v1/users/1 HTTP/1.1\r\nHost: api.example.com\r\nCookie: token=abc\r\n\r\n",
    "GET /api/v1/users/2 HTTP/1.1\r\nHost: api.example.com\r\nConnection: close\r\n\r\n",
    "GET /api/v1/orders HTTP/1.1\r\nHost: api.example.com\r\n\r\n",
    "GET /api/v1/orders/1 HTTP/1.1\r\nHost: api.example.com\r\n\r\n",
    "GET /api/v1/orders/2 HTTP/1.1\r\nHost: api.example.com\r\n\r\n",
    "GET /shop HTTP/1.1\r\nHost: shop.example.com\r\n\r\n",
    "GET /shop/category/electronics HTTP/1.1\r\nHost: shop.example.com\r\n\r\n",
    "GET /shop/category/clothing HTTP/1.1\r\nHost: shop.example.com\r\n\r\n",
    "GET /shop/product/1 HTTP/1.1\r\nHost: shop.example.com\r\nConnection: keep-alive\r\n\r\n",
    "GET /shop/product/2 HTTP/1.1\r\nHost: shop.example.com\r\nConnection: keep-alive\r\n\r\n",
    "GET /shop/product/3 HTTP/1.1\r\nHost: shop.example.com\r\nConnection: keep-alive\r\n\r\n",
    "GET /cart HTTP/1.1\r\nHost: shop.example.com\r\nCookie: cart=12345\r\n\r\n",
    "GET /checkout HTTP/1.1\r\nHost: shop.example.com\r\nConnection: close\r\n\r\n",
    "GET /notifications HTTP/1.1\r\nHost: example.com\r\nCookie: sessionid=abc123\r\n\r\n",
    "GET /messages HTTP/1.1\r\nHost: example.com\r\nConnection: keep-alive\r\n\r\n",
    "GET /messages/1 HTTP/1.1\r\nHost: example.com\r\n\r\n",
    "GET /messages/2 HTTP/1.1\r\nHost: example.com\r\n\r\n",
    "GET /api/v1/reports HTTP/1.1\r\nHost: api.example.com\r\nAccept: application/json\r\n\r\n",
    "GET /api/v1/reports/2026 HTTP/1.1\r\nHost: api.example.com\r\n\r\n",
    "GET /downloads/manual.pdf HTTP/1.1\r\nHost: downloads.example.com\r\n\r\n",
    "GET /downloads/app.exe HTTP/1.1\r\nHost: downloads.example.com\r\n\r\n",
    "GET /status HTTP/1.1\r\nHost: example.com\r\n\r\n",
    "GET /health HTTP/1.1\r\nHost: example.com\r\n\r\n",
    "GET /sitemap.xml HTTP/1.1\r\nHost: example.com\r\n\r\n",
    "GET /robots.txt HTTP/1.1\r\nHost: example.com\r\n\r\n",
    "GET /news HTTP/1.1\r\nHost: example.com\r\nConnection: keep-alive\r\n\r\n",
    "GET /news/1 HTTP/1.1\r\nHost: example.com\r\n\r\n",
    "GET /news/2 HTTP/1.1\r\nHost: example.com\r\n\r\n",
    "GET /news/3 HTTP/1.1\r\nHost: example.com\r\n\r\n",
    "GET /events HTTP/1.1\r\nHost: example.com\r\nConnection: keep-alive\r\n\r\n",
    "GET /events/1 HTTP/1.1\r\nHost: example.com\r\n\r\n",
    "GET /events/2 HTTP/1.1\r\nHost: example.com\r\n\r\n",
    "GET /gallery HTTP/1.1\r\nHost: gallery.example.com\r\n\r\n",
    "GET /gallery/photo1.jpg HTTP/1.1\r\nHost: gallery.example.com\r\n\r\n",
    "GET /gallery/photo2.jpg HTTP/1.1\r\nHost: gallery.example.com\r\n\r\n",
    "GET /gallery/photo3.jpg HTTP/1.1\r\nHost: gallery.example.com\r\n\r\n",
    "GET /support HTTP/1.1\r\nHost: support.example.com\r\n\r\n",
    "GET /support/ticket/1 HTTP/1.1\r\nHost: support.example.com\r\n\r\n",
    "GET /support/ticket/2 HTTP/1.1\r\nHost: support.example.com\r\n\r\n",
    "GET /support/ticket/3 HTTP/1.1\r\nHost: support.example.com\r\n\r\n",
    "GET /forum HTTP/1.1\r\nHost: forum.example.com\r\nConnection: keep-alive\r\n\r\n",
    "GET /forum/thread/1 HTTP/1.1\r\nHost: forum.example.com\r\n\r\n",
    "GET /forum/thread/2 HTTP/1.1\r\nHost: forum.example.com\r\n\r\n",
    "GET /forum/thread/3 HTTP/1.1\r\nHost: forum.example.com\r\n\r\n",
    "GET /forum/thread/4 HTTP/1.1\r\nHost: forum.example.com\r\n\r\n",
    "GET /forum/thread/5 HTTP/1.1\r\nHost: forum.example.com\r\n\r\n",
    "GET /forum/thread/6 HTTP/1.1\r\nHost: forum.example.com\r\n\r\n",
    "GET /forum/thread/7 HTTP/1.1\r\nHost: forum.example.com\r\n\r\n",
    "GET /forum/thread/8 HTTP/1.1\r\nHost: forum.example.com\r\n\r\n",
    "GET /forum/thread/9 HTTP/1.1\r\nHost: forum.example.com\r\n\r\n",
    "GET /forum/thread/10 HTTP/1.1\r\nHost: forum.example.com\r\n\r\n",
    "GET /blog/tag/tech HTTP/1.1\r\nHost: example.com\r\n\r\n",
    "GET /blog/tag/science HTTP/1.1\r\nHost: example.com\r\n\r\n",
    "GET /blog/tag/lifestyle HTTP/1.1\r\nHost: example.com\r\n\r\n",
    "POST /login HTTP/1.1\r\nHost: example.com\r\nContent-Type: application/x-www-form-urlencoded\r\nContent-Length: 27\r\n\r\nusername=abc&password=123",
    "POST /logout HTTP/1.1\r\nHost: example.com\r\nConnection: close\r\n\r\n",
    "POST /api/v1/items HTTP/1.1\r\nHost: api.example.com\r\nContent-Type: application/json\r\nContent-Length: 48\r\n\r\n{\"id\":1,\"name\":\"Item1\",\"price\":9.99}",
    "POST /api/v1/users HTTP/1.1\r\nHost: api.example.com\r\nContent-Type: application/json\r\nContent-Length: 36\r\n\r\n{\"id\":1,\"name\":\"Alice\"}",
    "POST /api/v1/orders HTTP/1.1\r\nHost: api.example.com\r\nContent-Type: application/json\r\nContent-Length: 48\r\n\r\n{\"order_id\":101,\"user_id\":1,\"total\":29.99}",
    "POST /contact HTTP/1.1\r\nHost: example.com\r\nContent-Type: text/plain\r\nContent-Length: 22\r\n\r\nHello, I need support",
    "POST /newsletter/subscribe HTTP/1.1\r\nHost: example.com\r\nContent-Type: application/x-www-form-urlencoded\r\nContent-Length: 20\r\n\r\nemail=user@example.com",
    "POST /shop/cart/add HTTP/1.1\r\nHost: shop.example.com\r\nContent-Type: application/json\r\nContent-Length: 32\r\n\r\n{\"product_id\":1,\"quantity\":2}",
    "POST /shop/cart/remove HTTP/1.1\r\nHost: shop.example.com\r\nContent-Type: application/json\r\nContent-Length: 32\r\n\r\n{\"product_id\":1,\"quantity\":2}",
    "POST /support/ticket HTTP/1.1\r\nHost: support.example.com\r\nContent-Type: text/plain\r\nContent-Length: 25\r\n\r\nNeed help with my account",
    "POST /api/v1/feedback HTTP/1.1\r\nHost: api.example.com\r\nContent-Type: application/json\r\nContent-Length: 38\r\n\r\n{\"user_id\":1,\"rating\":5,\"comments\":\"Great!\"}",
    "POST /api/v1/comments HTTP/1.1\r\nHost: api.example.com\r\nContent-Type: application/json\r\nContent-Length: 42\r\n\r\n{\"post_id\":10,\"user_id\":1,\"comment\":\"Nice post\"}",
    "POST /search HTTP/1.1\r\nHost: example.com\r\nContent-Type: application/x-www-form-urlencoded\r\nContent-Length: 14\r\n\r\nq=laptop",
    "POST /register HTTP/1.1\r\nHost: example.com\r\nContent-Type: application/x-www-form-urlencoded\r\nContent-Length: 29\r\n\r\nusername=bob&password=secret",
    "POST /checkout HTTP/1.1\r\nHost: shop.example.com\r\nContent-Type: application/json\r\nContent-Length: 38\r\n\r\n{\"cart_id\":1,\"payment_method\":\"card\"}",
    "POST /api/v1/login HTTP/1.1\r\nHost: api.example.com\r\nContent-Type: application/json\r\nContent-Length: 38\r\n\r\n{\"username\":\"user\",\"password\":\"pass\"}",
    "PUT /api/v1/items/1 HTTP/1.1\r\nHost: api.example.com\r\nContent-Type: application/json\r\nContent-Length: 32\r\n\r\n{\"name\":\"Updated Item 1\",\"price\":10.99}",
    "PUT /api/v1/items/2 HTTP/1.1\r\nHost: api.example.com\r\nContent-Type: application/json\r\nContent-Length: 32\r\n\r\n{\"name\":\"Updated Item 2\",\"price\":12.99}",
    "DELETE /api/v1/items/1 HTTP/1.1\r\nHost: api.example.com\r\n\r\n",
    "PATCH /api/v1/items/2 HTTP/1.1\r\nHost: api.example.com\r\nContent-Type: application/json\r\nContent-Length: 28\r\n\r\n{\"price\":14.99}",
    "OPTIONS /api/v1/items HTTP/1.1\r\nHost: api.example.com\r\n\r\n",
    "OPTIONS /api/v1/items HTTP/1.1\r\nHost: api.example.com\r\n\r\n"
};

const char *ls_http_request_line[100] = {
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
    ls_http_request_t req;
    memset(&req, 0, sizeof(req));
    req.raw_request = (const u_char *)BIG_REQ;
    req.request_len = strlen((const char *)req.raw_request);
    req.headers = malloc(sizeof(ls_header_t) * 32);
    req.header_count = 0;
    req.header_capacity = 32;
    int err_code = LS_ERR_OKAY;
    const u_char *end = req.raw_request + req.request_len;
    const u_char *cursor = req.raw_request;
    int state = 0;
    cursor = ls_http_parse_request(cursor, end, &req, &err_code, &state);
    if(req.method == LS_HTTP_OPTIONS) option_count++;
    free(req.headers);
}

#include <stdio.h>

void print_request_headers(const ls_header_t* headers, size_t header_count) {
    printf("\n===== Parsed Headers (%zu) =====\n", header_count);
    for (size_t i = 0; i < header_count; i++) {
        const ls_header_t* h = &headers[i];

        // Calculate lengths
        size_t name_len  = h->name_end - h->name_start;
        size_t value_len = h->value_end - h->value_start;

        // Print header id if you want
        printf("[%zu] Header ID: %u\n", i, h->header_id);

        // Print name and value slices
        printf("Name : %.*s\n", (int)name_len, h->name_start);
        printf("Value: %.*s\n", (int)value_len, h->value_start);

        printf("---------------------------\n");
    }
}

// Example: simple print function for parsed HTTP request
void print_parsed_request(const ls_http_request_t* req) {
    printf(">>>> Original Request <<<< \n\n%.*s\n<<<<End of original Request>>>>\n", (int)req->request_len, req->raw_request);

    const char* methods[] = {"GET", "PUT", "HEAD", "POST", "TRACE", "DELETE", "CONNECT", "OPTIONS"};
    printf("Method: %s\n", methods[req->method]);

    if(req->schema_start && req->schema_end)
        printf("Schema: %.*s\n", (int)(req->schema_end - req->schema_start), req->schema_start);

    if(req->host_start && req->host_end)
        printf("Host: %.*s\n", (int)(req->host_end - req->host_start), req->host_start);

    if(req->port_start && req->port_end)
        printf("Port: %.*s\n", (int)(req->port_end - req->port_start), req->port_start);

    if(req->path_start && req->path_end)
        printf("Path: %.*s\n", (int)(req->path_end - req->path_start), req->path_start);

    if(req->query_start && req->query_end)
        printf("Query: %.*s\n", (int)(req->query_end - req->query_start), req->query_start);

    if(req->version_start && req->version_end)
        printf("HTTP Version: %.*s\n", (int)(req->version_end - req->version_start), req->version_start);

    printf("HTTP Major: %u, Minor: %u\n", req->http_major, req->http_minor);

    printf("HTTP HEADER ID: %d\n", req->header_id);

    print_request_headers(req->headers, req->header_count);

    fflush(stdout);
}

// Test HTTP request array
const char* test_requests[] = {
    "GET / HTTP/1.1\r\n\r\n",
    "POST /submit HTTP/1.0\r\nContent-Length: 5\r\n\r\nHello",
    "PUT /resource/123?verbose=true HTTP/1.1\r\n\r\n",
    "HEAD /index.html HTTP/1.1\r\nHost: example.com\r\n\r\n",
    "DELETE /items/42 HTTP/1.1\r\n\r\n",
    "CONNECT www.example.com:443 HTTP/1.1\r\n\r\n",
    "OPTIONS * HTTP/1.1\r\n\r\n",
    "TRACE /debug HTTP/1.1\r\n\r\n",
    "GET http://example.com/path/to/resource?foo=bar&baz=qux HTTP/1.1\r\n\r\n",
    "GET https://[2001:db8::1]/path HTTP/1.1\r\n\r\n",
    "GET /path/with%20spaces?query=1+2 HTTP/1.1\r\n\r\n",
    "GET /complex/path/?a=1&b=2&c=3 HTTP/2.0\r\n\r\n"
};

int testFunc() {
    size_t n_requests = 10;
    unsigned int capacity = 32;
    for(size_t i = 0; i < n_requests; ++i) {
        ls_http_request_t req;
        memset(&req, 0, sizeof(req));
        req.raw_request = (const u_char*)BIG_REQ;
        req.headers = malloc(sizeof(ls_header_t) * capacity);
        req.header_capacity = capacity;
        req.request_len = strlen(req.raw_request);
        int err_code = LS_ERR_OKAY;
        const u_char *end = req.raw_request + req.request_len;
        const u_char *cursor = req.raw_request;
        void *parser_state = NULL;
        int state = 0;
        cursor = ls_http_parse_request(cursor, end, &req, &err_code, &state);
        print_parsed_request(&req);
        printf("--------------------------------------------------\n");
    }
    return 0;
}

int main()
{

    ls_http_parser_init();
    ls_http_init_header_hash();

    // testFunc();
    // return 0;

    const int num_tests = 1;
    const int num_iterations = 500000;

    for (int i = 0; i < num_tests; ++i)
    {
        count = 0;
        for (int j = 0; j < num_iterations; ++j)
        {
            test_request_parse();
            count++;
        }
    }
    printf("OPTION COUNT: %ld \n", option_count);
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
//             ls_http_request_t req;
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