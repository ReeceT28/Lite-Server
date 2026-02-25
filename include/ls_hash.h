#pragma once
#include<stdlib.h>
#include "ls_types.h"
/* TO DO: Create interface for storing and managing hash tables for http headers and maybe other things */

/* Considering using gperf or a custom thing to generate perfect hash tables? Or maybe a perfect hash table for the headers that will almost always need processing? */

typedef struct {
    void*           entry;
    
} lt_hash_entry_t;

typedef struct {
    lt_hash_entry_t**   buckets;
    unsigned int            size; 
} lt_hash_table_t;

inline unsigned int lt_header_hash(const u_char* str, unsigned int len);
int lt_init_hash_table(lt_hash_table_t* table, unsigned int exponent);
void* find_hash(lt_hash_table_t* table, unsigned int hash);