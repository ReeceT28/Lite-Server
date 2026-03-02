#pragma once
#include<stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdint.h>
#include <nmmintrin.h> 
#include "ls_types.h"
/* TO DO: Create interface for storing and managing hash tables for http headers and maybe other things */
/* Considering using gperf or a custom thing to generate perfect hash tables? Or maybe a perfect hash table for the headers that will almost always need processing? */

typedef unsigned char u_char;

typedef struct {
    const char *name;
    uint16_t    len;
    unsigned long hash;
    int header_id;
} ls_hdr_hash_entry_t;


/* Initialize the hash table (call once at startup) */
void ls_http_init_header_hash(void);

/* Lookup a header by name using its hash and length */
int ls_hdr_hash_lookup(const u_char *start, size_t len, unsigned long hash);

unsigned long crc32_hash(const char* name);