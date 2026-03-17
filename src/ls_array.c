#include "ls_array.h"

ls_array_t* ls_create_array(ls_mem_pool_t* pool, size_t size, size_t n)
{
    ls_array_t* array = ls_palloc(pool, sizeof(ls_array_t));
    if(array == NULL) {
        return NULL;
    }
    void* head = ls_palloc(pool, n * size);

    if(head == NULL) {
        return NULL;
    }

    array->head = head;
    array->n_alocted = n;
    array->n_elmnts = 0;
    array->mem_pool = pool;

    return array; 
}

void* ls_array_push(ls_array_t* array)
{
    /* Check if the array is full */
    if(array->n_elmnts = array->n_alocted) {
        /* If there is space after the array then we can just use that */
        // if(array->mem_pool)
    }
    return array->head + array->size * array->n_elmnts++;
}