#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "ls_http_request.h"
#include "ls_http_parser.h"
#include "ls_http_parser_test.h"

static const char* BIG_REQ =  "GET /wp-content/uploads/2010/03/hello-kitty-darth-vader-pink.jpg HTTP/1.1\r\n"
    "Host: www.kittyhell.com\r\n"
    "User-Agent: Mozilla/5.0 (Macintosh; U; Intel Mac OS X 10.6; ja-JP-mac; rv:1.9.2.3) Gecko/20100401 Firefox/3.6.3 " 
    "Pathtraq/0.9\r\n"
    "Accept: text/html,application/xhtml+xml,application/xml;q=0.9,*/*;q=0.8\r\n"  
    "Accept-Language: ja,en-us;q=0.7,en;q=0.3\r\n" 
    "Accept-Encoding: gzip,deflate\r\n"
    "Accept-Charset: Shift_JIS,utf-8;q=0.7,*;q=0.7\r\n"
    "Keep-Alive: 115\r\n"
    "Connection: keep-alive\r\n"
    "Cookie: wp_ozh_wsa_visits=2; wp_ozh_wsa_visit_lasttime=xxxxxxxxxx; "
    "__utma=xxxxxxxxx.xxxxxxxxxx.xxxxxxxxxx.xxxxxxxxxx.xxxxxxxxxx.x; "
    "__utmz=xxxxxxxxx.xxxxxxxxxx.x.x.utmccn=(referral)|utmcsr=reader.livedoor.com|utmcct=/reader/|utmcmd=referral\r\n"
    "\r\n";

static const char* ls_http_request_complete[100] = {
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

static void print_request_headers(const ls_header_t* headers, size_t header_count)
{
    printf("\n===== Parsed Headers (%zu) =====\n", header_count);
    for (size_t i = 0; i < header_count; i++) {
        const ls_header_t* h = &headers[i];

        size_t name_len  = h->name_end - h->name_start;
        size_t value_len = h->value_end - h->value_start;

        printf("[%zu] Header ID: %u\n", i, h->header_id);

        printf("Name : %.*s\n", (int)name_len, h->name_start);
        printf("Value: %.*s\n", (int)value_len, h->value_start);

        printf("---------------------------\n");
    }
}

static void print_parsed_request(const ls_http_request_t* req)
{
    printf(">>>> Original Request <<<< \n\n%.*s\n>>>> End of original Request <<<<\n", (int)req->request_len, req->raw_request);

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

    print_request_headers(req->headers, req->header_count);

}

int parser_run_benchmark(int iterations)
{
    size_t option_count = 0;

    for (int i = 0; i < iterations; ++i) {

        ls_http_request_t req;
        memset(&req, 0, sizeof(req));

        req.raw_request = (const u_char *)BIG_REQ;
        req.request_len = strlen((const char *)req.raw_request);

        req.headers = malloc(sizeof(ls_header_t) * 32);
        req.header_capacity = 32;

        int err_code = LS_ERR_OKAY;
        const u_char *end = req.raw_request + req.request_len;
        const u_char *cursor = req.raw_request;
        int state = 0;

        cursor = ls_http_parse_request(cursor, end,
                                       &req, &err_code, &state);

        if (req.method == LS_HTTP_OPTIONS)
            option_count++;

        free(req.headers);
    }

    printf("OPTION COUNT: %zu\n", option_count);
    return 0;
}

int parser_run_print_test(void)
{
    size_t n_requests = 10;
    unsigned int capacity = 32;
    for(size_t i = 0; i < n_requests; ++i) {
        ls_http_request_t req;
        memset(&req, 0, sizeof(req));
        req.raw_request = (const u_char*)BIG_REQ;
        req.headers = malloc(sizeof(ls_header_t) * capacity);
        req.header_capacity = capacity;
        req.request_len = strlen((const char*)req.raw_request);
        int err_code = LS_ERR_OKAY;
        const u_char *end = req.raw_request + req.request_len;
        const u_char *cursor = req.raw_request;
        int state = 0;
        cursor = ls_http_parse_request(cursor, end, &req, &err_code, &state);
        print_parsed_request(&req);
        printf("--------------------------------------------------\n");
        free(req.headers);
    }
    return 0;
}