#pragma once
#include "ls_mem_pool.h"
typedef struct ls_array_s{
    void* head;
    size_t n_elmnts;
    size_t n_alocted;
    ls_mem_pool_t* mem_pool;
    size_t size;
} ls_array_t;

ls_array_t init_array(ls_array_t* array, ls_mem_pool_t* pool, size_t n, size_t size);
ls_array_t* ls_create_array(ls_mem_pool_t* pool, size_t size, size_t n);
void* ls_array_push(ls_array_t* array);
/* Create function for reserving more space */
void ls_array_reserve();