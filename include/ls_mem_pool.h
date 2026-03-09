#pragma once
#include <stdlib.h>
#include <stdint.h>
#include "ls_types.h"


extern size_t ls_page_size;

/* 16 is nice for alignment - MUST be power of 2 or you WILL BREAK align_ptr in ls_mem_pool.c */
#define LS_POOL_ALIGNMENT 16
#define LS_DEFAULT_BLOCK_SIZE 4096
/* Will experiment with this */
#define LS_MAX_POOL_ALLOC ls_page_size - 1


typedef struct ls_pool_block_s {
    u_char* next_free;
    u_char* end;
    struct ls_pool_block_s* next;
    int failed;
} ls_pool_block_t;

typedef struct ls_pool_large_s {
    struct ls_pool_large_s* next;
    void* alloc;
} ls_pool_large_t;

typedef struct ls_mem_pool_s {
    ls_pool_block_t* head;
    ls_pool_block_t* current;
    ls_pool_large_t* large_head;
    ls_pool_large_t* curr_large_alloc;
    size_t block_size;
} ls_mem_pool_t;

ls_mem_pool_t* ls_init_mem_pool(size_t size);
void ls_free_pool(ls_mem_pool_t* pool);
void ls_init_alloc();
void* ls_palloc(ls_mem_pool_t* pool, size_t size);
