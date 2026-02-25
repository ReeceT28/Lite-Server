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

/* Basic hash for http headers */
inline unsigned int lt_header_hash(const u_char* str, unsigned int len)
{
    unsigned int hash = 0;
    for(unsigned int i=0; i < len; ++i){
        hash = hash * 31 + str[i] | 0x20; 
    }
    return hash;
} 

int lt_init_hash_table(lt_hash_table_t* table, unsigned int exponent)
{
    if(exponent > 15) {
        /* 2^15 = 32768, currently only using for http header, will never need this many */
        return -1;
    }
    table->size = 1<<exponent;
    
    for(unsigned int i=0; i < table->size ; ++i) {
        table->buckets[i] = NULL;
    }

    return 0;
}

void* find_hash(lt_hash_table_t* table, unsigned int hash)
{
    lt_hash_entry_t* entry = table->buckets[hash & (table->size - 1 )];
    if(entry == NULL){
        return NULL;
    }
}