#include <stdio.h>
#include <strings.h>
#include <string.h>
#include "ls_http_request.h"
#include "ls_hash.h"


/* Some common headers for benchmarking */
static const char *common_http_request_headers[] = {
    "Host",
    "Connection",
    "User-Agent",
    "Accept",
    "Accept-Encoding",
    "Accept-Language",
    "Upgrade-Insecure-Requests",
    "Sec-Fetch-Site",
    "Sec-Fetch-Mode",
    "Sec-Fetch-Dest",
    "Sec-Fetch-User",
    "Sec-CH-UA",
    "Sec-CH-UA-Mobile",
    "Sec-CH-UA-Platform",
    "Cookie",
    "Referer",
    "Origin",
    "Content-Type",
    "Content-Length",
    "Authorization"
};

static const size_t common_http_request_headers_count =
    sizeof(common_http_request_headers) / sizeof(common_http_request_headers[0]);

#define LS_HDR_HASH_TABLE_SIZE 32
static ls_hdr_hash_entry_t header_hash_table[LS_HDR_HASH_TABLE_SIZE];


static void ls_hdr_hash_insert(const char* name, int header_id)
{
    unsigned long hash = 0;
    size_t len = strlen(name);

    for (size_t i = 0; i < len; ++i) {
        char c = name[i];
        if ((unsigned char)(c - 'A') <= ('Z' - 'A'))
            c |= 0x20;
        hash = ((hash<<5)+hash) ^ c;
    }

    unsigned idx = hash & (LS_HDR_HASH_TABLE_SIZE - 1);

    while (header_hash_table[idx].name)
        idx = (idx + 1) & (LS_HDR_HASH_TABLE_SIZE - 1);

    header_hash_table[idx].name = name;
    header_hash_table[idx].len = len;
    header_hash_table[idx].hash = hash;
    header_hash_table[idx].header_id = header_id;
}

int ls_hdr_hash_lookup(const u_char *start,
                       size_t len,
                       unsigned long hash)
{
    unsigned idx = hash & (LS_HDR_HASH_TABLE_SIZE - 1);

    for (;;) {
        ls_hdr_hash_entry_t *e = &header_hash_table[idx];

        if (!e->name)
            return LS_HTTP_HDR_UNKOWN;

        if (e->hash == hash &&
            e->len == len &&
            memcmp(start, e->name, len) == 0)
            return e->header_id;

        idx = (idx + 1) & (LS_HDR_HASH_TABLE_SIZE - 1);
    }
}

/* Initialize all headers once at startup */
void ls_http_init_header_hash(void)
{
    memset(header_hash_table, 0, sizeof(header_hash_table));

    ls_hdr_hash_insert("Host",       LS_HTTP_HDR_HOST);
    ls_hdr_hash_insert("Accept",     LS_HTTP_HDR_ACCEPT);
    ls_hdr_hash_insert("Connection", LS_HTTP_HDR_CONNECTION);
    ls_hdr_hash_insert("Referer",    LS_HTTP_HDR_REFERER);
    ls_hdr_hash_insert("Cookie",     LS_HTTP_HDR_COOKIE);
    ls_hdr_hash_insert("Origin",     LS_HTTP_HDR_ORIGIN);
    ls_hdr_hash_insert("User-Agent",       LS_HTTP_HDR_USER_AGENT);
    ls_hdr_hash_insert("Accept-Encoding",  LS_HTTP_HDR_ACCEPT_ENCODING);
    ls_hdr_hash_insert("Accept-Language",  LS_HTTP_HDR_ACCEPT_LANGUAGE);
    ls_hdr_hash_insert("Content-Type",     LS_HTTP_HDR_CONTENT_TYPE);
    ls_hdr_hash_insert("Content-Length",   LS_HTTP_HDR_CONTENT_LENGTH);
    ls_hdr_hash_insert("Keep-Alive",       LS_HTTP_HDR_KEEP_ALIVE);
    ls_hdr_hash_insert("Authorization",    LS_HTTP_HDR_AUTHORIZATION);
}