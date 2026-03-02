#include <stdio.h>
#include <strings.h>
#include <string.h>
#include "ls_http_request.h"
#include "ls_hash.h"

#define LS_HDR_HASH_TABLE_SIZE 64
static ls_hdr_hash_entry_t header_hash_table[LS_HDR_HASH_TABLE_SIZE];

unsigned long crc32_hash(const char* name) {
    unsigned long crc = 0xFFFFFFFF; 
    for (size_t i = 0; name[i]; i++) {
        char c = name[i];
        crc = _mm_crc32_u8(crc, c);
    }
    return crc ^ 0xFFFFFFFF; 
}

static void ls_hdr_hash_insert(const char* name, int header_id)
{
    unsigned long hash = crc32_hash(name);

    unsigned idx = hash & (LS_HDR_HASH_TABLE_SIZE - 1);

    while (header_hash_table[idx].name)
        idx = (idx + 1) & (LS_HDR_HASH_TABLE_SIZE - 1);

    header_hash_table[idx].name = name;
    header_hash_table[idx].len = strlen(name);
    header_hash_table[idx].hash = hash;
    header_hash_table[idx].header_id = header_id;
    // printf("Index of %s is: %d\n", name, idx);
}
/* Assume perfect hash table */
int ls_hdr_hash_lookup( unsigned long hash)
{
    unsigned idx = hash & (LS_HDR_HASH_TABLE_SIZE - 1);
    ls_hdr_hash_entry_t *e = &header_hash_table[idx];
    return e->header_id;
}

/* Initialize all headers once at startup */
void ls_http_init_header_hash(void)
{
    memset(header_hash_table, 0, sizeof(header_hash_table));

    ls_hdr_hash_insert("host",       LS_HTTP_HDR_HOST);
    ls_hdr_hash_insert("accept",     LS_HTTP_HDR_ACCEPT);
    ls_hdr_hash_insert("connection", LS_HTTP_HDR_CONNECTION);
    ls_hdr_hash_insert("referer",    LS_HTTP_HDR_REFERER);
    ls_hdr_hash_insert("cookie",     LS_HTTP_HDR_COOKIE);
    ls_hdr_hash_insert("origin",     LS_HTTP_HDR_ORIGIN);
    ls_hdr_hash_insert("user-agent",       LS_HTTP_HDR_USER_AGENT);
    ls_hdr_hash_insert("accept-encoding",  LS_HTTP_HDR_ACCEPT_ENCODING);
    ls_hdr_hash_insert("accept-language",  LS_HTTP_HDR_ACCEPT_LANGUAGE);
    ls_hdr_hash_insert("content-type",     LS_HTTP_HDR_CONTENT_TYPE);
    ls_hdr_hash_insert("content-length",   LS_HTTP_HDR_CONTENT_LENGTH);
    ls_hdr_hash_insert("keep-alive",       LS_HTTP_HDR_KEEP_ALIVE);
    ls_hdr_hash_insert("authorization",    LS_HTTP_HDR_AUTHORIZATION);
}

// static const u_char* parse_header_name_hash_v2( const u_char* cursor, const u_char* end, ls_http_request_t* req,
//     int* err_code, int* state)
// {
//     while (cursor < end) {
//         u_char c = header_chars[*cursor];
// 
//         if (!c) {
//             if (*cursor == ':') {
//                 size_t len = cursor - req->header_name_start;
//                 req->header_name_end = cursor;
// 
//                 // finalize CRC32
//                 req->header_hash ^= 0xFFFFFFFF;
// 
//                 req->header_id = ls_hdr_hash_lookup(
//                     req->header_name_start,
//                     len,
//                     req->header_hash);
// 
//                 *state = LS_HTTP_OWS_BEFORE_VALUE;
//                 return ++cursor;
//             }
// 
//             *err_code = LS_ERR_BAD_REQUEST;
//             return NULL;
//         }
// 
//         req->header_hash = _mm_crc32_u8(req->header_hash, c);
// 
//         cursor++;
//     }
// 
//     *err_code = LS_ERR_NEED_MORE_CHARS;
//     return cursor;
// }