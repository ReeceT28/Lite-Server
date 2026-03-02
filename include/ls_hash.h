#pragma once
#include <stdint.h>
#include <nmmintrin.h> 

typedef struct {
    const char *name;
    uint16_t    len;
    unsigned long hash;
    int header_id;
} ls_hdr_hash_entry_t;


/* Initialize the hash table (call once at startup) */
void ls_http_init_header_hash(void);

/* Lookup a header by name using its hash and length */
int ls_hdr_hash_lookup(unsigned long hash);

unsigned long crc32_hash(const char* name);