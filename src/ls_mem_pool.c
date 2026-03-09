#include <stdint.h>
#include <stdlib.h>
#include <malloc.h>
#include <unistd.h>
#include "ls_mem_pool.h"

size_t ls_page_size;

void ls_init_alloc()
{
    ls_page_size = sysconf(_SC_PAGE_SIZE);
}


/* Cool fast trick for alignment when a is a power of 2 */
static inline uintptr_t align_ptr(uintptr_t p, size_t a)
{
    /* (p + (a-1)) ensures that we can never go backwards when aligning possibly writing over data, we do + (a-1) so if p is already aligned we don't waste memory */
    /* Example is best for & ~(a-1). Say a = 16 then a-1 = 0b00001111 => ~(a-1) = 0b11110000 if we & with these notice that the result will always be a multiple of a */
    return (p + (a - 1)) & ~(a - 1);
}


void ls_free_pool(ls_mem_pool_t* pool)
{
    /* free large allocations */
    ls_pool_large_t* large_ptr = pool->large_head;
    while (large_ptr) {
        ls_pool_large_t* next = large_ptr->next;
        free(large_ptr->alloc);
        free(large_ptr);
        large_ptr = next;
    }

    /* free additional blocks (skip first) */
    ls_pool_block_t* block_ptr = pool->head->next;
    while (block_ptr) {
        ls_pool_block_t* next = block_ptr->next;
        free(block_ptr);
        block_ptr = next;
    }

    /* free the original pool memory */
    free(pool);
}

ls_mem_pool_t* ls_init_mem_pool(size_t size)
{
    ls_mem_pool_t* pool;

    size_t block_size = align_ptr((uintptr_t)size, LS_POOL_ALIGNMENT);

    void *mem = aligned_alloc(LS_POOL_ALIGNMENT, block_size);

    if(mem == NULL) {
        return NULL;
    }

    pool = (ls_mem_pool_t*)mem;

    uintptr_t block_start = align_ptr((uintptr_t)((u_char*)mem + sizeof(ls_mem_pool_t)), LS_POOL_ALIGNMENT);
    ls_pool_block_t* block = (ls_pool_block_t*)block_start;
    block->next_free = (u_char *)align_ptr(block_start + sizeof(ls_pool_block_t), LS_POOL_ALIGNMENT);
    block->end = (u_char *)mem + block_size;
    block->next = NULL;
    block->failed = 0;

    pool->head = block;
    pool->current = block;
    pool->large_head = NULL;
    pool->curr_large_alloc = NULL;
    pool->block_size = block_size;

    return pool;
}

static void* alloc_block(ls_mem_pool_t* pool)
{
    // printf("Allocating new block \n");
    size_t block_size = pool->block_size;
    // printf("Block size: %zu\n", block_size);
    ls_pool_block_t* block = (ls_pool_block_t*)aligned_alloc(LS_POOL_ALIGNMENT, block_size);
    if(block == NULL) {
        return NULL;
    }
    pool->current->next = block;
    pool->current = block;
    block->next_free = (u_char*)block + sizeof(ls_pool_block_t);
    block->failed = 0;
    block->next = NULL;
    block->end = (u_char*)block + block_size; 
    return block->next_free;
}

static void* small_palloc(ls_mem_pool_t* pool, size_t size)
{
    ls_pool_block_t* block = pool->current;
    uintptr_t aligned = align_ptr((uintptr_t)block->next_free, LS_POOL_ALIGNMENT);
    // printf("Free space: %zu \n", (uintptr_t)block->end - aligned);
    if((uintptr_t)block->end - aligned >= size) {
        block->next_free = (u_char*)aligned + size;
        return (void*)aligned;
    }
    else {
        // Alternative approach is we can track number of failed allocs per block and move on if we get too many but still try ones with not too many fails.
        return alloc_block(pool);
    }

}

static void* large_palloc(ls_mem_pool_t* pool, size_t size)
{
    ls_pool_large_t* lrg_ptr = malloc(sizeof(ls_pool_large_t));
    if(lrg_ptr == NULL) return NULL;
    lrg_ptr->alloc = malloc(size);
    if(lrg_ptr->alloc == NULL) { 
        free(lrg_ptr);
        return NULL;
    }
    lrg_ptr->next = NULL;

    if(pool->curr_large_alloc) {
        pool->curr_large_alloc->next = lrg_ptr;
        pool->curr_large_alloc = lrg_ptr;
    }
    else {
        pool->large_head = lrg_ptr;
        pool->curr_large_alloc = lrg_ptr;
    }

    return lrg_ptr->alloc;
}

void* ls_palloc(ls_mem_pool_t* pool, size_t size)
{
    if(size < LS_MAX_POOL_ALLOC) {
        return small_palloc(pool, size);
    }
    else {
        return large_palloc(pool, size);
    }
}